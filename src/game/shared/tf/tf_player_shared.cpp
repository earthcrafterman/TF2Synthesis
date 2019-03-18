﻿//====== Copyright © 1996-2004, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================
#include "cbase.h"
#include "tf_gamerules.h"
#include "tf_player_shared.h"
#include "takedamageinfo.h"
#include "tf_weaponbase.h"
#include "effect_dispatch_data.h"
#include "tf_item.h"
#include "entity_capture_flag.h"
#include "baseobject_shared.h"
#include "tf_weapon_medigun.h"
#include "tf_weapon_pipebomblauncher.h"
#include "in_buttons.h"
#include "tf_viewmodel.h"
#include "econ_wearable.h"
#include "tf_weapon_invis.h"
#include "tf_weapon_buff_item.h"

// Client specific.
#ifdef CLIENT_DLL
#include "c_tf_player.h"
#include "c_te_effect_dispatch.h"
#include "c_tf_fx.h"
#include "soundenvelope.h"
#include "c_tf_playerclass.h"
#include "iviewrender.h"
#include "engine/ivdebugoverlay.h"
#include "c_tf_playerresource.h"
#include "c_tf_team.h"
#include "prediction.h"

#define CTFPlayerClass C_TFPlayerClass

// Server specific.
#else
#include "tf_player.h"
#include "te_effect_dispatch.h"
#include "tf_fx.h"
#include "util.h"
#include "tf_team.h"
#include "tf_gamestats.h"
#include "tf_playerclass.h"
#include "tf_weapon_builder.h"
#endif

ConVar tf_spy_invis_time( "tf_spy_invis_time", "1.0", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "Transition time in and out of spy invisibility", true, 0.1, true, 5.0 );
ConVar tf_spy_invis_unstealth_time( "tf_spy_invis_unstealth_time", "2.0", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "Transition time in and out of spy invisibility", true, 0.1, true, 5.0 );

ConVar tf_spy_max_cloaked_speed( "tf_spy_max_cloaked_speed", "999", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED );	// no cap
ConVar tf_max_health_boost( "tf_max_health_boost", "1.5", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "Max health factor that players can be boosted to by healers.", true, 1.0, false, 0 );
ConVar tf_invuln_time( "tf_invuln_time", "1.0", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "Time it takes for invulnerability to wear off." );
ConVar tf_soldier_buff_pulses( "tf_soldier_buff_pulses", "10", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "Time it takes for buff to wear off." );

#ifdef GAME_DLL
ConVar tf_boost_drain_time( "tf_boost_drain_time", "15.0", FCVAR_DEVELOPMENTONLY, "Time it takes for a full health boost to drain away from a player.", true, 0.1, false, 0 );
ConVar tf_debug_bullets( "tf_debug_bullets", "0", FCVAR_CHEAT, "Visualize bullet traces." );
ConVar tf_damage_events_track_for( "tf_damage_events_track_for", "30", FCVAR_DEVELOPMENTONLY );
#endif

ConVar tf_useparticletracers( "tf_useparticletracers", "1", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "Use particle tracers instead of old style ones." );
ConVar tf_spy_cloak_consume_rate( "tf_spy_cloak_consume_rate", "10.0", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "cloak to use per second while cloaked, from 100 max )" );	// 10 seconds of invis
ConVar tf_spy_cloak_regen_rate( "tf_spy_cloak_regen_rate", "3.3", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "cloak to regen per second, up to 100 max" );		// 30 seconds to full charge
ConVar tf_spy_cloak_no_attack_time( "tf_spy_cloak_no_attack_time", "2.0", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED, "time after uncloaking that the spy is prohibited from attacking" );

//ConVar tf_spy_stealth_blink_time( "tf_spy_stealth_blink_time", "0.3", FCVAR_DEVELOPMENTONLY, "time after being hit the spy blinks into view" );
//ConVar tf_spy_stealth_blink_scale( "tf_spy_stealth_blink_scale", "0.85", FCVAR_DEVELOPMENTONLY, "percentage visible scalar after being hit the spy blinks into view" );

ConVar tf_tournament_hide_domination_icons( "tf_tournament_hide_domination_icons", "0", FCVAR_REPLICATED, "Tournament mode server convar that forces clients to not display the domination icons above players dominating them." );

ConVar tf_damage_disablespread( "tf_damage_disablespread", "1", FCVAR_NOTIFY | FCVAR_REPLICATED, "Toggles the random damage spread applied to all player damage." );
ConVar tf_always_loser( "tf_always_loser", "0", FCVAR_REPLICATED | FCVAR_CHEAT, "Force loserstate to true." );

ConVar sv_showimpacts( "sv_showimpacts", "0", FCVAR_REPLICATED, "Shows client (red) and server (blue) bullet impact point (1=both, 2=client-only, 3=server-only)" );
ConVar sv_showplayerhitboxes("sv_showplayerhitboxes", "0", FCVAR_REPLICATED, "Show lag compensated hitboxes for the specified player index whenever a player fires." );

ConVar tf2c_building_hauling( "tf2c_building_hauling", "1", FCVAR_REPLICATED, "Toggle Engineer's building hauling ability." );
ConVar tf2c_disable_player_shadows( "tf2c_disable_player_shadows", "0", FCVAR_REPLICATED, "Disables rendering of player shadows regardless of client's graphical settings." );

#ifdef GAME_DLL
extern ConVar tf2c_random_weapons;
#endif

#define TF_SPY_STEALTH_BLINKTIME   0.3f
#define TF_SPY_STEALTH_BLINKSCALE  0.85f

#define TF_PLAYER_CONDITION_CONTEXT	"TFPlayerConditionContext"

#define MAX_DAMAGE_EVENTS		128

#define TF_BUFF_RADIUS			450.0f

const char *g_pszBDayGibs[22] =
{
	"models/effects/bday_gib01.mdl",
	"models/effects/bday_gib02.mdl",
	"models/effects/bday_gib03.mdl",
	"models/effects/bday_gib04.mdl",
	"models/player/gibs/gibs_balloon.mdl",
	"models/player/gibs/gibs_burger.mdl",
	"models/player/gibs/gibs_boot.mdl",
	"models/player/gibs/gibs_bolt.mdl",
	"models/player/gibs/gibs_can.mdl",
	"models/player/gibs/gibs_clock.mdl",
	"models/player/gibs/gibs_fish.mdl",
	"models/player/gibs/gibs_gear1.mdl",
	"models/player/gibs/gibs_gear2.mdl",
	"models/player/gibs/gibs_gear3.mdl",
	"models/player/gibs/gibs_gear4.mdl",
	"models/player/gibs/gibs_gear5.mdl",
	"models/player/gibs/gibs_hubcap.mdl",
	"models/player/gibs/gibs_licenseplate.mdl",
	"models/player/gibs/gibs_spring1.mdl",
	"models/player/gibs/gibs_spring2.mdl",
	"models/player/gibs/gibs_teeth.mdl",
	"models/player/gibs/gibs_tire.mdl"
};

#ifdef CLIENT_DLL
EXTERN_RECV_TABLE( DT_ScriptCreatedItem )
#else
EXTERN_SEND_TABLE( DT_ScriptCreatedItem )
#endif
//=============================================================================
//
// Tables.
//

// Client specific.
#ifdef CLIENT_DLL

BEGIN_RECV_TABLE_NOBASE( CTFPlayerShared, DT_TFPlayerSharedLocal )
RecvPropInt( RECVINFO( m_nDesiredDisguiseTeam ) ),
RecvPropInt( RECVINFO( m_nDesiredDisguiseClass ) ),
RecvPropTime( RECVINFO( m_flStealthNoAttackExpire ) ),
RecvPropTime( RECVINFO( m_flStealthNextChangeTime ) ),
RecvPropFloat(  RECVINFO( m_flCloakMeter ) ),
RecvPropArray3( RECVINFO_ARRAY( m_bPlayerDominated ), RecvPropBool( RECVINFO( m_bPlayerDominated[0] ) ) ),
RecvPropArray3( RECVINFO_ARRAY( m_bPlayerDominatingMe ), RecvPropBool( RECVINFO( m_bPlayerDominatingMe[0] ) ) ),
RecvPropInt( RECVINFO( m_iDesiredWeaponID ) ),
RecvPropArray3( RECVINFO_ARRAY( m_nStreaks ), RecvPropInt( RECVINFO( m_nStreaks[0] ) ) ),
RecvPropArray3( RECVINFO_ARRAY( m_flCondExpireTimeLeft ), RecvPropFloat(RECVINFO( m_flCondExpireTimeLeft[0] ) ) ),
END_RECV_TABLE()

BEGIN_RECV_TABLE_NOBASE( CTFPlayerShared, DT_TFPlayerShared )
RecvPropInt( RECVINFO( m_nPlayerCond ) ),
RecvPropInt( RECVINFO( m_nPlayerCondEx ) ),
RecvPropInt( RECVINFO( m_nPlayerCondEx2 ) ),
RecvPropInt( RECVINFO( m_nPlayerCondEx3 ) ),
RecvPropInt( RECVINFO( m_bJumping ) ),
RecvPropInt( RECVINFO( m_nNumHealers ) ),
RecvPropInt( RECVINFO( m_iCritMult ) ),
RecvPropInt( RECVINFO( m_bAirDash ) ),
RecvPropInt( RECVINFO( m_nAirDucked ) ),
RecvPropInt( RECVINFO( m_nPlayerState ) ),
RecvPropInt( RECVINFO( m_iDesiredPlayerClass ) ),
RecvPropTime( RECVINFO( m_flStunExpireTime ) ),
RecvPropInt( RECVINFO( m_nStunFlags ) ),
RecvPropFloat( RECVINFO( m_flStunMovementSpeed ) ),
RecvPropFloat( RECVINFO( m_flStunResistance ) ),
RecvPropEHandle( RECVINFO( m_hStunner ) ),
RecvPropEHandle( RECVINFO( m_hCarriedObject ) ),
RecvPropBool( RECVINFO( m_bCarryingObject ) ),
RecvPropInt( RECVINFO( m_nTeamTeleporterUsed ) ),
RecvPropBool( RECVINFO( m_bArenaSpectator ) ),
RecvPropBool( RECVINFO( m_bGunslinger ) ),
RecvPropInt( RECVINFO( m_iRespawnParticleID ) ),
RecvPropInt( RECVINFO( m_iMaxHealth ) ),
RecvPropFloat( RECVINFO( m_flEffectBarProgress ) ),
// Spy.
RecvPropTime( RECVINFO( m_flInvisChangeCompleteTime ) ),
RecvPropInt( RECVINFO( m_nDisguiseTeam ) ),
RecvPropInt( RECVINFO( m_nDisguiseClass ) ),
RecvPropInt( RECVINFO( m_nMaskClass ) ),
RecvPropInt( RECVINFO( m_iDisguiseTargetIndex) ),
RecvPropInt( RECVINFO( m_iDisguiseHealth ) ),
RecvPropInt( RECVINFO( m_iDisguiseMaxHealth ) ),
RecvPropFloat( RECVINFO( m_flDisguiseChargeLevel ) ),
RecvPropDataTable( RECVINFO_DT( m_DisguiseItem ), 0, &REFERENCE_RECV_TABLE( DT_ScriptCreatedItem ) ),
// Local Data.
RecvPropDataTable( "tfsharedlocaldata", 0, 0, &REFERENCE_RECV_TABLE( DT_TFPlayerSharedLocal ) ),
END_RECV_TABLE()

BEGIN_PREDICTION_DATA_NO_BASE( CTFPlayerShared )
DEFINE_PRED_FIELD( m_nPlayerState, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
DEFINE_PRED_FIELD( m_nPlayerCond, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
DEFINE_PRED_FIELD( m_nPlayerCondEx, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
DEFINE_PRED_FIELD( m_nPlayerCondEx2, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
DEFINE_PRED_FIELD( m_nPlayerCondEx3, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
DEFINE_PRED_FIELD( m_flCloakMeter, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
DEFINE_PRED_FIELD( m_bJumping, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
DEFINE_PRED_FIELD( m_bAirDash, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
DEFINE_PRED_FIELD( m_nAirDucked, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
DEFINE_PRED_FIELD( m_flInvisChangeCompleteTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
DEFINE_PRED_FIELD( m_iDesiredWeaponID, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
DEFINE_PRED_FIELD( m_iRespawnParticleID, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
DEFINE_PRED_FIELD( m_flEffectBarProgress, FIELD_FLOAT, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()

// Server specific.
#else

BEGIN_SEND_TABLE_NOBASE( CTFPlayerShared, DT_TFPlayerSharedLocal )
SendPropInt( SENDINFO( m_nDesiredDisguiseTeam ), 3, SPROP_UNSIGNED ),
SendPropInt( SENDINFO( m_nDesiredDisguiseClass ), 4, SPROP_UNSIGNED ),
SendPropTime( SENDINFO( m_flStealthNoAttackExpire ) ),
SendPropTime( SENDINFO( m_flStealthNextChangeTime ) ),
SendPropFloat( SENDINFO( m_flCloakMeter ), 0, SPROP_NOSCALE | SPROP_CHANGES_OFTEN, 0.0, 100.0 ),
SendPropArray3( SENDINFO_ARRAY3( m_bPlayerDominated ), SendPropBool( SENDINFO_ARRAY( m_bPlayerDominated ) ) ),
SendPropArray3( SENDINFO_ARRAY3( m_bPlayerDominatingMe ), SendPropBool( SENDINFO_ARRAY( m_bPlayerDominatingMe ) ) ),
SendPropInt( SENDINFO( m_iDesiredWeaponID ) ),
SendPropArray3( SENDINFO_ARRAY3( m_nStreaks ), SendPropInt( SENDINFO_ARRAY( m_nStreaks ) ) ),
SendPropArray3( SENDINFO_ARRAY3( m_flCondExpireTimeLeft ), SendPropFloat( SENDINFO_ARRAY( m_flCondExpireTimeLeft ) ) ),
END_SEND_TABLE()

BEGIN_SEND_TABLE_NOBASE( CTFPlayerShared, DT_TFPlayerShared )
SendPropInt( SENDINFO( m_nPlayerCond ), -1, SPROP_UNSIGNED | SPROP_CHANGES_OFTEN ),
SendPropInt( SENDINFO( m_nPlayerCondEx ), -1, SPROP_UNSIGNED | SPROP_CHANGES_OFTEN ),
SendPropInt( SENDINFO( m_nPlayerCondEx2 ), -1, SPROP_UNSIGNED | SPROP_CHANGES_OFTEN ),
SendPropInt( SENDINFO( m_nPlayerCondEx3 ), -1, SPROP_UNSIGNED | SPROP_CHANGES_OFTEN ),
SendPropInt( SENDINFO( m_bJumping ), 1, SPROP_UNSIGNED | SPROP_CHANGES_OFTEN ),
SendPropInt( SENDINFO( m_nNumHealers ), 5, SPROP_UNSIGNED | SPROP_CHANGES_OFTEN ),
SendPropInt( SENDINFO( m_iCritMult ), 8, SPROP_UNSIGNED | SPROP_CHANGES_OFTEN ),
SendPropInt( SENDINFO( m_bAirDash ), 1, SPROP_UNSIGNED | SPROP_CHANGES_OFTEN ),
SendPropInt( SENDINFO( m_nAirDucked ), 2, SPROP_UNSIGNED | SPROP_CHANGES_OFTEN ),
SendPropInt( SENDINFO( m_nPlayerState ), Q_log2( TF_STATE_COUNT ) + 1, SPROP_UNSIGNED ),
SendPropInt( SENDINFO( m_iDesiredPlayerClass ), Q_log2( TF_CLASS_COUNT_ALL ) + 1, SPROP_UNSIGNED ),
SendPropTime( SENDINFO( m_flStunExpireTime ) ),
SendPropInt( SENDINFO( m_nStunFlags ), -1, SPROP_UNSIGNED ),
SendPropFloat( SENDINFO( m_flStunMovementSpeed ), 0, SPROP_NOSCALE ),
SendPropFloat( SENDINFO( m_flStunResistance ), 0, SPROP_NOSCALE ),
SendPropEHandle( SENDINFO( m_hStunner ) ),
SendPropEHandle( SENDINFO(m_hCarriedObject ) ),
SendPropBool( SENDINFO( m_bCarryingObject ) ),
SendPropInt( SENDINFO( m_nTeamTeleporterUsed ), 3, SPROP_UNSIGNED ),
SendPropBool( SENDINFO( m_bArenaSpectator ) ),
SendPropBool( SENDINFO( m_bGunslinger ) ),
SendPropInt( SENDINFO( m_iRespawnParticleID ), 0, SPROP_UNSIGNED ),
SendPropInt( SENDINFO( m_iMaxHealth ), 10 ),
SendPropFloat( SENDINFO( m_flEffectBarProgress ), 11, 0, 0.0f, 100.0f ),
// Spy
SendPropTime( SENDINFO( m_flInvisChangeCompleteTime ) ),
SendPropInt( SENDINFO( m_nDisguiseTeam ), 3, SPROP_UNSIGNED ),
SendPropInt( SENDINFO( m_nDisguiseClass ), 4, SPROP_UNSIGNED ),
SendPropInt( SENDINFO( m_nMaskClass ), 4, SPROP_UNSIGNED ),
SendPropInt( SENDINFO( m_iDisguiseTargetIndex ), 7, SPROP_UNSIGNED ),
SendPropInt( SENDINFO( m_iDisguiseHealth ), 10 ),
SendPropInt( SENDINFO( m_iDisguiseMaxHealth ), 10 ),
SendPropFloat( SENDINFO(m_flDisguiseChargeLevel ), 0, SPROP_NOSCALE ),
SendPropDataTable( SENDINFO_DT( m_DisguiseItem ), &REFERENCE_SEND_TABLE( DT_ScriptCreatedItem ) ),
// Local Data.
SendPropDataTable( "tfsharedlocaldata", 0, &REFERENCE_SEND_TABLE( DT_TFPlayerSharedLocal ), SendProxy_SendLocalDataTable ),
END_SEND_TABLE()

#endif


// --------------------------------------------------------------------------------------------------- //
// Shared CTFPlayer implementation.
// --------------------------------------------------------------------------------------------------- //

// --------------------------------------------------------------------------------------------------- //
// CTFPlayerShared implementation.
// --------------------------------------------------------------------------------------------------- //

CTFPlayerShared::CTFPlayerShared()
{
	m_nPlayerState.Set( TF_STATE_WELCOME );
	m_bJumping = false;
	m_bAirDash = false;
	m_nAirDucked = 0;
	m_flStealthNoAttackExpire = 0.0f;
	m_flStealthNextChangeTime = 0.0f;
	m_iCritMult = 0;
	m_flInvisibility = 0.0f;

	m_iDesiredWeaponID = -1;
	m_iRespawnParticleID = 0;

	m_iStunPhase = 0;

	m_nTeamTeleporterUsed = TEAM_UNASSIGNED;

	m_bArenaSpectator = false;

	m_bGunslinger = false;

#ifdef CLIENT_DLL
	m_iDisguiseWeaponModelIndex = -1;
	m_pDisguiseWeaponInfo = NULL;
	m_pCritSound = NULL;
	m_pCritEffect = NULL;
#else
	memset( m_flChargeOffTime, 0, sizeof( m_flChargeOffTime ) );
	memset( m_bChargeSounds, 0, sizeof( m_bChargeSounds ) );
#endif
}

void CTFPlayerShared::Init( CTFPlayer *pPlayer )
{
	m_pOuter = pPlayer;

	m_flNextBurningSound = 0;

	SetJumping( false );
}

//-----------------------------------------------------------------------------
// Purpose: Add a condition and duration
// duration of PERMANENT_CONDITION means infinite duration
//-----------------------------------------------------------------------------
void CTFPlayerShared::AddCond( int nCond, float flDuration /* = PERMANENT_CONDITION */ )
{
	Assert( nCond >= 0 && nCond < TF_COND_LAST );
	int nCondFlag = nCond;
	int *pVar = NULL;
	if ( nCond < 96 )
	{
		if ( nCond < 64 )
		{
			if ( nCond < 32 )
			{
				pVar = &m_nPlayerCond.GetForModify();
			}
			else
			{
				pVar = &m_nPlayerCondEx.GetForModify();
				nCondFlag -= 32;
			}
		}
		else
		{
			pVar = &m_nPlayerCondEx2.GetForModify();
			nCondFlag -= 64;
		}
	}
	else
	{
		pVar = &m_nPlayerCondEx3.GetForModify();
		nCondFlag -= 96;
	}

	*pVar |= ( 1 << nCondFlag );
	m_flCondExpireTimeLeft.Set( nCond, flDuration );
	OnConditionAdded( nCond );
}

//-----------------------------------------------------------------------------
// Purpose: Forcibly remove a condition
//-----------------------------------------------------------------------------
void CTFPlayerShared::RemoveCond(int nCond)
{
	Assert(nCond >= 0 && nCond < TF_COND_LAST);

	int nCondFlag = nCond;
	int *pVar = NULL;
	if (nCond < 96)
	{
		if (nCond < 64)
		{
			if (nCond < 32)
			{
				pVar = &m_nPlayerCond.GetForModify();
			}
			else
			{
				pVar = &m_nPlayerCondEx.GetForModify();
				nCondFlag -= 32;
			}
		}
		else
		{
			pVar = &m_nPlayerCondEx2.GetForModify();
			nCondFlag -= 64;
		}
	}
	else
	{
		pVar = &m_nPlayerCondEx3.GetForModify();
		nCondFlag -= 96;
	}

	*pVar &= ~(1 << nCondFlag);
	m_flCondExpireTimeLeft.Set(nCond, 0);

	OnConditionRemoved(nCond);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFPlayerShared::InCond(int nCond)
{
	Assert(nCond >= 0 && nCond < TF_COND_LAST);

	int nCondFlag = nCond;
	const int *pVar = NULL;
	if (nCond < 96)
	{
		if (nCond < 64)
		{
			if (nCond < 32)
			{
				pVar = &m_nPlayerCond.Get();
			}
			else
			{
				pVar = &m_nPlayerCondEx.Get();
				nCondFlag -= 32;
			}
		}
		else
		{
			pVar = &m_nPlayerCondEx2.Get();
			nCondFlag -= 64;
		}
	}
	else
	{
		pVar = &m_nPlayerCondEx3.Get();
		nCondFlag -= 96;
	}

	return ((*pVar & (1 << nCondFlag)) != 0);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
float CTFPlayerShared::GetConditionDuration( int nCond )
{
	Assert(nCond >= 0 && nCond < TF_COND_LAST);

	if (InCond(nCond))
	{
		return m_flCondExpireTimeLeft[nCond];
	}

	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayerShared::IsCritBoosted( void )
{
	// Oh man...
	if ( InCond( TF_COND_CRITBOOSTED ) ||
		InCond( TF_COND_CRITBOOSTED_PUMPKIN ) ||
		InCond( TF_COND_CRITBOOSTED_USER_BUFF ) ||
		InCond( TF_COND_CRITBOOSTED_DEMO_CHARGE ) ||
		InCond( TF_COND_CRITBOOSTED_FIRST_BLOOD ) ||
		InCond( TF_COND_CRITBOOSTED_BONUS_TIME ) ||
		InCond( TF_COND_CRITBOOSTED_CTF_CAPTURE ) ||
		InCond( TF_COND_CRITBOOSTED_ON_KILL ) ||
		InCond( TF_COND_CRITBOOSTED_CARD_EFFECT ) ||
		InCond( TF_COND_CRITBOOSTED_RUNE_TEMP ) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayerShared::IsMiniCritBoosted( void )
{
	if ( InCond( TF_COND_OFFENSEBUFF ) ||
		InCond( TF_COND_ENERGY_BUFF ) ||
		InCond( TF_COND_MINICRITBOOSTED_ON_KILL ) )
		return true;
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayerShared::IsSpeedBoosted( void )
{
	if ( InCond( TF_COND_SPEED_BOOST ) ||
		InCond( TF_COND_HALLOWEEN_SPEED_BOOST ) )
		return true;
	return false;
}

void CTFPlayerShared::DebugPrintConditions(void)
{
#ifndef CLIENT_DLL
	const char *szDll = "Server";
#else
	const char *szDll = "Client";
#endif

	Msg("( %s ) Conditions for player ( %d )\n", szDll, m_pOuter->entindex());

	int i;
	int iNumFound = 0;
	for (i = 0; i<TF_COND_LAST; i++)
	{
		if (InCond(i))
		{
			if (m_flCondExpireTimeLeft[i] == PERMANENT_CONDITION)
			{
				Msg("( %s ) Condition %d - ( permanent cond )\n", szDll, i);
			}
			else
			{
				Msg("( %s ) Condition %d - ( %.1f left )\n", szDll, i, m_flCondExpireTimeLeft[i]);
			}

			iNumFound++;
		}
	}

	if (iNumFound == 0)
	{
		Msg("( %s ) No active conditions\n", szDll);
	}
}

#ifdef CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnPreDataChanged(void)
{
	m_nOldConditions = m_nPlayerCond;
	m_nOldConditionsEx = m_nPlayerCondEx;
	m_nOldConditionsEx2 = m_nPlayerCondEx2;
	m_nOldConditionsEx3 = m_nPlayerCondEx3;
	m_nOldDisguiseClass = GetDisguiseClass();
	m_nOldDisguiseTeam = GetDisguiseTeam();
	m_iOldDisguiseWeaponModelIndex = m_iDisguiseWeaponModelIndex;
	m_iOldDisguiseWeaponID = m_DisguiseItem.GetItemDefIndex();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnDataChanged(void)
{
	// Update conditions from last network change
	SyncConditions(m_nPlayerCond, m_nOldConditions, 0, 0);
	SyncConditions(m_nPlayerCondEx, m_nOldConditionsEx, 0, 32);
	SyncConditions(m_nPlayerCondEx2, m_nOldConditionsEx2, 0, 64);
	SyncConditions(m_nPlayerCondEx3, m_nOldConditionsEx3, 0, 96);

	m_nOldConditions = m_nPlayerCond;
	m_nOldConditionsEx = m_nPlayerCondEx;
	m_nOldConditionsEx2 = m_nPlayerCondEx2;
	m_nOldConditionsEx3 = m_nPlayerCondEx3;

	if (m_nOldDisguiseClass != GetDisguiseClass() || m_nOldDisguiseTeam != GetDisguiseTeam())
	{
		OnDisguiseChanged();
	}

	if (m_iOldDisguiseWeaponID != m_DisguiseItem.GetItemDefIndex())
	{
		RecalcDisguiseWeapon();
	}

	if (m_iDisguiseWeaponModelIndex != m_iOldDisguiseWeaponModelIndex)
	{
		C_BaseCombatWeapon *pWeapon = m_pOuter->GetActiveWeapon();

		if (pWeapon)
		{
			pWeapon->SetModelIndex(pWeapon->GetWorldModelIndex());
		}
	}

	if (IsLoser())
	{
		C_BaseCombatWeapon *pWeapon = m_pOuter->GetActiveWeapon();
		if (pWeapon && !pWeapon->IsEffectActive(EF_NODRAW))
		{
			pWeapon->SetWeaponVisible(false);
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: check the newly networked conditions for changes
//-----------------------------------------------------------------------------
void CTFPlayerShared::SyncConditions(int nCond, int nOldCond, int nUnused, int iOffset)
{
	if (nCond == nOldCond)
		return;

	int nCondChanged = nCond ^ nOldCond;
	int nCondAdded = nCondChanged & nCond;
	int nCondRemoved = nCondChanged & nOldCond;

	int i;
	for (i = 0; i < 32; i++)
	{
		if ( nCondAdded & ( 1 << i ) )
		{
			OnConditionAdded( i + iOffset );
		}
		else if ( nCondRemoved & ( 1 << i ) )
		{
			OnConditionRemoved( i + iOffset );
		}
	}
}

#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
// Purpose: Remove any conditions affecting players
//-----------------------------------------------------------------------------
void CTFPlayerShared::RemoveAllCond(CTFPlayer *pPlayer)
{
	int i;
	for (i = 0; i < TF_COND_LAST; i++)
	{
		if (InCond(i))
		{
			RemoveCond(i);
		}
	}

	// Now remove all the rest
	m_nPlayerCond = 0;
	m_nPlayerCondEx = 0;
	m_nPlayerCondEx2 = 0;
	m_nPlayerCondEx3 = 0;
}


//-----------------------------------------------------------------------------
// Purpose: Called on both client and server. Server when we add the bit,
// and client when it recieves the new cond bits and finds one added
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnConditionAdded(int nCond)
{
	switch (nCond)
	{
	case TF_COND_HEALTH_BUFF:
#ifdef GAME_DLL
		m_flHealFraction = 0;
		m_flDisguiseHealFraction = 0;
#endif
		break;

	case TF_COND_STEALTHED:
		OnAddStealthed();
		break;

	case TF_COND_INVULNERABLE:
		OnAddInvulnerable();
		break;

	case TF_COND_TELEPORTED:
		OnAddTeleported();
		break;

	case TF_COND_DISGUISING:
		OnAddDisguising();
		break;

	case TF_COND_DISGUISED:
		OnAddDisguised();
		break;

	case TF_COND_TAUNTING:
		OnAddTaunting();
		break;

	case TF_COND_CRITBOOSTED:
	case TF_COND_CRITBOOSTED_PUMPKIN:
	case TF_COND_CRITBOOSTED_USER_BUFF:
	case TF_COND_CRITBOOSTED_DEMO_CHARGE:
	case TF_COND_CRITBOOSTED_FIRST_BLOOD:
	case TF_COND_CRITBOOSTED_BONUS_TIME:
	case TF_COND_CRITBOOSTED_CTF_CAPTURE:
	case TF_COND_CRITBOOSTED_ON_KILL:
	case TF_COND_CRITBOOSTED_CARD_EFFECT:
	case TF_COND_CRITBOOSTED_RUNE_TEMP:
		OnAddCritboosted();
		break;

	case TF_COND_BURNING:
		OnAddBurning();
		break;

	case TF_COND_HEALTH_OVERHEALED:
	case TF_COND_DISGUISE_HEALTH_OVERHEALED:
#ifdef CLIENT_DLL
		m_pOuter->UpdateOverhealEffect();
#endif
		break;

	case TF_COND_STUNNED:
		OnAddStunned();
		break;

	case TF_COND_DISGUISED_AS_DISPENSER:
		m_pOuter->TeamFortress_SetSpeed();
		break;

	case TF_COND_HALLOWEEN_GIANT:
		OnAddHalloweenGiant();
		break;

	case TF_COND_HALLOWEEN_TINY:
		OnAddHalloweenTiny();
		break;

	case TF_COND_URINE:
		OnAddUrine();
		break;

	case TF_COND_PHASE:
		OnAddPhase();
		break;

	case TF_COND_SPEED_BOOST:
	case TF_COND_HALLOWEEN_SPEED_BOOST:
		OnAddSpeedBoost();
		break;

	case TF_COND_OFFENSEBUFF:
	case TF_COND_DEFENSEBUFF:
	case TF_COND_REGENONDAMAGEBUFF:
		OnAddBuff();
		break;

	default:
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Called on both client and server. Server when we remove the bit,
// and client when it recieves the new cond bits and finds one removed
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnConditionRemoved(int nCond)
{
	switch (nCond)
	{
	case TF_COND_ZOOMED:
		OnRemoveZoomed();
		break;

	case TF_COND_HEALTH_BUFF:
#ifdef GAME_DLL
		m_flHealFraction = 0;
		m_flDisguiseHealFraction = 0;
#endif
		break;

	case TF_COND_STEALTHED:
		OnRemoveStealthed();
		break;

	case TF_COND_DISGUISED:
		OnRemoveDisguised();
		break;

	case TF_COND_DISGUISING:
		OnRemoveDisguising();
		break;

	case TF_COND_INVULNERABLE:
		OnRemoveInvulnerable();
		break;

	case TF_COND_TELEPORTED:
		OnRemoveTeleported();
		break;

	case TF_COND_TAUNTING:
		OnRemoveTaunting();
		break;

	case TF_COND_CRITBOOSTED:
	case TF_COND_CRITBOOSTED_PUMPKIN:
	case TF_COND_CRITBOOSTED_USER_BUFF:
	case TF_COND_CRITBOOSTED_DEMO_CHARGE:
	case TF_COND_CRITBOOSTED_FIRST_BLOOD:
	case TF_COND_CRITBOOSTED_BONUS_TIME:
	case TF_COND_CRITBOOSTED_CTF_CAPTURE:
	case TF_COND_CRITBOOSTED_ON_KILL:
	case TF_COND_CRITBOOSTED_CARD_EFFECT:
	case TF_COND_CRITBOOSTED_RUNE_TEMP:
		OnRemoveCritboosted();
		break;

	case TF_COND_BURNING:
		OnRemoveBurning();
		break;

	case TF_COND_HEALTH_OVERHEALED:
	case TF_COND_DISGUISE_HEALTH_OVERHEALED:
#ifdef CLIENT_DLL
		m_pOuter->UpdateOverhealEffect();
#endif

	case TF_COND_STUNNED:
		OnRemoveStunned();
		break;

	case TF_COND_HALLOWEEN_GIANT:
		OnRemoveHalloweenGiant();
		break;

	case TF_COND_HALLOWEEN_TINY:
		OnRemoveHalloweenTiny();
		break;

	case TF_COND_URINE:
		OnRemoveUrine();
		break;

	case TF_COND_PHASE:
		OnRemovePhase();
		break;

	case TF_COND_OFFENSEBUFF:
	case TF_COND_DEFENSEBUFF:
	case TF_COND_REGENONDAMAGEBUFF:
		OnRemoveBuff();
		break;

	default:
		break;
	}
}

int CTFPlayerShared::GetMaxBuffedHealth( void )
{
	float flBoostMax = m_pOuter->GetMaxHealth() * tf_max_health_boost.GetFloat();

	int iRoundDown = floor( flBoostMax / 5 );
	iRoundDown = iRoundDown * 5;

	return iRoundDown;
}

int CTFPlayerShared::GetMaxHealth( void )
{
	return m_iMaxHealth;
}

int	CTFPlayerShared::GetDisguiseMaxHealth( void )
{
	return GetPlayerClassData( m_nDisguiseClass )->m_nMaxHealth;
}

int CTFPlayerShared::GetDisguiseMaxBuffedHealth( void )
{
	float flBoostMax = GetDisguiseMaxHealth() * tf_max_health_boost.GetFloat();

	int iRoundDown = floor( flBoostMax / 5 );
	iRoundDown = iRoundDown * 5;

	return iRoundDown;
}

//-----------------------------------------------------------------------------
// Purpose: Runs SERVER SIDE only Condition Think
// If a player needs something to be updated no matter what do it here (invul, etc).
//-----------------------------------------------------------------------------
void CTFPlayerShared::ConditionGameRulesThink(void)
{
#ifdef GAME_DLL
	if ( m_flNextCritUpdate < gpGlobals->curtime )
	{
		UpdateCritMult();
		m_flNextCritUpdate = gpGlobals->curtime + 0.5;
	}

	int i;
	for (i = 0; i < TF_COND_LAST; i++)
	{
		if ( InCond( i ) )
		{
			// Ignore permanent conditions
			if ( m_flCondExpireTimeLeft[i] != PERMANENT_CONDITION )
			{
				float flReduction = gpGlobals->frametime;

				if ( ConditionExpiresFast( i ) )
				{
					// If we're being healed, we reduce bad conditions faster
					if ( m_aHealers.Count() > 0 )
					{
						if ( i == TF_COND_URINE )
							flReduction *= m_aHealers.Count() + 1;
						else
							flReduction += ( m_aHealers.Count() * flReduction * 4 );
					}
				}

				m_flCondExpireTimeLeft.Set( i, max( m_flCondExpireTimeLeft[i] - flReduction, 0 ) );

				if ( m_flCondExpireTimeLeft[i] == 0 )
				{
					RemoveCond( i );
				}
			}
		}
	}

	// Our health will only decay ( from being medic buffed ) if we are not being healed by a medic
	// Dispensers can give us the TF_COND_HEALTH_BUFF, but will not maintain or give us health above 100%s
	bool bDecayHealth = true;

	// If we're being healed, heal ourselves
	if ( InCond( TF_COND_HEALTH_BUFF ) )
	{
		// Heal faster if we haven't been in combat for a while
		float flTimeSinceDamage = gpGlobals->curtime - m_pOuter->GetLastDamageTime();
		float flScale = RemapValClamped( flTimeSinceDamage, 10, 15, 1.0, 3.0 );

		bool bHasFullHealth = m_pOuter->GetHealth() >= m_pOuter->GetMaxHealth();

		float fTotalHealAmount = 0.0f;
		for ( int i = 0; i < m_aHealers.Count(); i++ )
		{
			// Dispensers refill cloak.
			if ( m_aHealers[i].bDispenserHeal )
			{
				m_flCloakMeter = min( m_flCloakMeter + m_aHealers[i].flAmount * gpGlobals->frametime, 100.0f );
			}

			// Dispensers don't heal above 100%
			if ( bHasFullHealth && m_aHealers[i].bDispenserHeal )
			{
				continue;
			}

			// Being healed by a medigun, don't decay our health
			bDecayHealth = false;

			// Dispensers heal at a constant rate
			if ( m_aHealers[i].bDispenserHeal )
			{
				// Dispensers heal at a slower rate, but ignore flScale
				m_flHealFraction += gpGlobals->frametime * m_aHealers[i].flAmount;
			}
			else	// player heals are affected by the last damage time
			{
				m_flHealFraction += gpGlobals->frametime * m_aHealers[i].flAmount * flScale;
			}

			fTotalHealAmount += m_aHealers[i].flAmount;
		}

		int nHealthToAdd = ( int )m_flHealFraction;
		if ( nHealthToAdd > 0 )
		{
			m_flHealFraction -= nHealthToAdd;

			int iBoostMax = GetMaxBuffedHealth();

			if ( InCond( TF_COND_DISGUISED ) )
			{
				// Separate cap for disguised health
				//int iFakeBoostMax = GetDisguiseMaxBuffedHealth();
				//int nFakeHealthToAdd = clamp(nHealthToAdd, 0, iFakeBoostMax - m_iDisguiseHealth);
				//m_iDisguiseHealth += nFakeHealthToAdd;
				AddDisguiseHealth( nHealthToAdd, true );
			}

			// Cap it to the max we'll boost a player's health
			nHealthToAdd = clamp( nHealthToAdd, 0, iBoostMax - m_pOuter->GetHealth() );

			m_pOuter->TakeHealth( nHealthToAdd, DMG_IGNORE_MAXHEALTH );

			// split up total healing based on the amount each healer contributes
			for ( int i = 0; i < m_aHealers.Count(); i++ )
			{
				if ( m_aHealers[i].pPlayer.IsValid() )
				{
					CTFPlayer *pPlayer = static_cast< CTFPlayer  *>( static_cast< CBaseEntity  *>( m_aHealers[i].pPlayer ) );
					if ( IsAlly( pPlayer ) )
					{
						CTF_GameStats.Event_PlayerHealedOther( pPlayer, nHealthToAdd * ( m_aHealers[i].flAmount / fTotalHealAmount ) );
					}
					else
					{
						CTF_GameStats.Event_PlayerLeachedHealth( m_pOuter, m_aHealers[i].bDispenserHeal, nHealthToAdd * ( m_aHealers[i].flAmount / fTotalHealAmount ) );
					}
				}
			}
		}

		if ( InCond( TF_COND_BURNING ) )
		{
			// Reduce the duration of this burn 
			float flReduction = 2;	 // ( flReduction + 1 ) x faster reduction
			m_flFlameRemoveTime -= flReduction * gpGlobals->frametime;
		}
	}

	if ( bDecayHealth )
	{
		// If we're not being buffed, our health drains back to our max
		if ( m_pOuter->GetHealth() > m_pOuter->GetMaxHealth() )
		{
			float flBoostMaxAmount = GetMaxBuffedHealth() - m_pOuter->GetMaxHealth();
			m_flHealFraction += ( gpGlobals->frametime * ( flBoostMaxAmount / tf_boost_drain_time.GetFloat() ) );

			int nHealthToDrain = ( int )m_flHealFraction;
			if ( nHealthToDrain > 0 )
			{
				m_flHealFraction -= nHealthToDrain;

				// Manually subtract the health so we don't generate pain sounds / etc
				m_pOuter->m_iHealth -= nHealthToDrain;
			}
		}

		if ( InCond( TF_COND_DISGUISED ) && m_iDisguiseHealth > m_iDisguiseMaxHealth )
		{
			float flBoostMaxAmount = GetDisguiseMaxBuffedHealth() - m_iDisguiseMaxHealth;
			m_flDisguiseHealFraction += ( gpGlobals->frametime * ( flBoostMaxAmount / tf_boost_drain_time.GetFloat() ) );

			int nHealthToDrain = ( int )m_flDisguiseHealFraction;
			if ( nHealthToDrain > 0 )
			{
				m_flDisguiseHealFraction -= nHealthToDrain;

				// Reduce our fake disguised health by roughly the same amount
				m_iDisguiseHealth -= nHealthToDrain;
			}
		}
	}

	// Overheal effects
	if ( m_pOuter->GetHealth() > m_pOuter->GetMaxHealth() )
	{
		if ( !InCond( TF_COND_HEALTH_OVERHEALED ) )
		{
			AddCond( TF_COND_HEALTH_OVERHEALED );
		}
	}
	else
	{
		if ( InCond( TF_COND_HEALTH_OVERHEALED ) )
		{
			RemoveCond( TF_COND_HEALTH_OVERHEALED );
		}
	}

	// Overheal effects for disguised spies
	if ( InCond( TF_COND_DISGUISED ) && m_iDisguiseHealth > m_iDisguiseMaxHealth )
	{
		if ( !InCond( TF_COND_DISGUISE_HEALTH_OVERHEALED ) )
		{
			AddCond( TF_COND_DISGUISE_HEALTH_OVERHEALED );
		}
	}
	else
	{
		if ( InCond( TF_COND_DISGUISE_HEALTH_OVERHEALED ) )
		{
			RemoveCond( TF_COND_DISGUISE_HEALTH_OVERHEALED );
		}
	}

	// Taunt
	if ( InCond( TF_COND_TAUNTING ) )
	{
		if ( gpGlobals->curtime > m_flTauntRemoveTime )
		{
			m_pOuter->ResetTauntHandle();

			//m_pOuter->SnapEyeAngles( m_pOuter->m_angTauntCamera );
			//m_pOuter->SetAbsAngles( m_pOuter->m_angTauntCamera );
			//m_pOuter->SetLocalAngles( m_pOuter->m_angTauntCamera );

			RemoveCond( TF_COND_TAUNTING );
		}
	}

	if ( InCond( TF_COND_BURNING ) && ( m_pOuter->m_flPowerPlayTime < gpGlobals->curtime ) )
	{
		// If we're underwater, put the fire out
		if ( gpGlobals->curtime > m_flFlameRemoveTime || m_pOuter->GetWaterLevel() >= WL_Waist )
		{
			RemoveCond( TF_COND_BURNING );
		}
		else if ( ( gpGlobals->curtime >= m_flFlameBurnTime ) && ( TF_CLASS_PYRO != m_pOuter->GetPlayerClass()->GetClassIndex() ) )
		{
			float flBurnDamage = TF_BURNING_DMG;
			CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( m_hBurnWeapon, flBurnDamage, mult_wpn_burndmg );

			// Burn the player (if not pyro, who does not take persistent burning damage)
			CTakeDamageInfo info( m_hBurnAttacker, m_hBurnAttacker, m_hBurnWeapon, flBurnDamage, DMG_BURN | DMG_PREVENT_PHYSICS_FORCE, TF_DMG_CUSTOM_BURNING );
			m_pOuter->TakeDamage( info );
			m_flFlameBurnTime = gpGlobals->curtime + TF_BURNING_FREQUENCY;
		}

		if ( m_flNextBurningSound < gpGlobals->curtime )
		{
			m_pOuter->SpeakConceptIfAllowed( MP_CONCEPT_ONFIRE );
			m_flNextBurningSound = gpGlobals->curtime + 2.5;
		}
	}

	if (InCond( TF_COND_URINE ) && m_pOuter->GetWaterLevel() >= WL_Waist )
	{
		RemoveCond( TF_COND_URINE );
	}

	if ( InCond(TF_COND_DISGUISING ) )
	{
		if ( gpGlobals->curtime > m_flDisguiseCompleteTime )
		{
			CompleteDisguise();
		}
	}

	// Stops the drain hack.
	if ( m_pOuter->IsPlayerClass( TF_CLASS_MEDIC ) || tf2c_random_weapons.GetBool() )
	{
		CWeaponMedigun *pWeapon = m_pOuter->GetMedigun();
		if ( pWeapon && pWeapon->IsReleasingCharge() )
		{
			pWeapon->DrainCharge();
		}
	}

	TestAndExpireChargeEffect( TF_CHARGE_INVULNERABLE );
	TestAndExpireChargeEffect( TF_CHARGE_CRITBOOSTED );

	if ( InCond( TF_COND_STEALTHED_BLINK ) )
	{
		if (TF_SPY_STEALTH_BLINKTIME/*tf_spy_stealth_blink_time.GetFloat()*/ < ( gpGlobals->curtime - m_flLastStealthExposeTime ) )
		{
			RemoveCond( TF_COND_STEALTHED_BLINK );
		}
	}

	if ( InCond( TF_COND_STUNNED ) )
	{
		if ( gpGlobals->curtime > m_flStunExpireTime )
		{
			// Only check stun phase if we're unable to move
			if ( !( m_nStunFlags & TF_STUNFLAG_BONKSTUCK ) || m_iStunPhase == STUN_PHASE_END )
			{
				RemoveCond( TF_COND_STUNNED );
			}
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Do CLIENT/SERVER SHARED condition thinks.
//-----------------------------------------------------------------------------
void CTFPlayerShared::ConditionThink( void )
{
	bool bIsLocalPlayer = false;
#ifdef CLIENT_DLL
	bIsLocalPlayer = m_pOuter->IsLocalPlayer();
#else
	bIsLocalPlayer = true;
#endif

	if ( m_pOuter->IsPlayerClass( TF_CLASS_SPY ) && bIsLocalPlayer )
	{
		if ( InCond( TF_COND_STEALTHED ) )
		{
			m_flCloakMeter -= gpGlobals->frametime * tf_spy_cloak_consume_rate.GetFloat();

			if ( m_flCloakMeter <= 0.0f )
			{
				FadeInvis( tf_spy_invis_unstealth_time.GetFloat() );
			}
		}		
		else
		{
			m_flCloakMeter += gpGlobals->frametime * tf_spy_cloak_regen_rate.GetFloat();

			if  (m_flCloakMeter >= 100.0f )
			{
				m_flCloakMeter = 100.0f;
			}
		}
	}

	if ( InCond( TF_COND_PHASE ) )
	{
		if ( gpGlobals->curtime > m_flPhaseTime )
		{
			UpdatePhaseEffects();

			// limit how often we can update in case of spam
			m_flPhaseTime = gpGlobals->curtime + 0.25f;
		}
	}

	UpdateRageBuffsAndRage();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveZoomed(void)
{
#ifdef GAME_DLL
	m_pOuter->SetFOV(m_pOuter, 0, 0.1f);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnAddDisguising(void)
{
#ifdef CLIENT_DLL
	if ( m_pOuter->m_pDisguisingEffect )
	{
		m_pOuter->ParticleProp()->StopEmission( m_pOuter->m_pDisguisingEffect );
	}

	if ( ( !m_pOuter->IsLocalPlayer() || !m_pOuter->InFirstPersonView() ) && ( !InCond( TF_COND_STEALTHED ) || !m_pOuter->IsEnemyPlayer() ) )
	{
		const char *pszEffectName = ConstructTeamParticle( "spy_start_disguise_%s", m_pOuter->GetTeamNumber() );

		m_pOuter->m_pDisguisingEffect = m_pOuter->ParticleProp()->Create( pszEffectName, PATTACH_ABSORIGIN_FOLLOW );
		m_pOuter->m_flDisguiseEffectStartTime = gpGlobals->curtime;
	}

	m_pOuter->EmitSound( "Player.Spy_Disguise" );

#endif
}

//-----------------------------------------------------------------------------
// Purpose: set up effects for when player finished disguising
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnAddDisguised(void)
{
#ifdef CLIENT_DLL
	if ( m_pOuter->m_pDisguisingEffect )
	{
		// turn off disguising particles
		m_pOuter->ParticleProp()->StopEmission( m_pOuter->m_pDisguisingEffect );
		m_pOuter->m_pDisguisingEffect = NULL;
	}
	m_pOuter->m_flDisguiseEndEffectStartTime = gpGlobals->curtime;
#endif
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: start, end, and changing disguise classes
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnDisguiseChanged( void )
{
	// recalc disguise model index
	//RecalcDisguiseWeapon();
	m_pOuter->UpdateRecentlyTeleportedEffect();
	UpdateCritBoostEffect();
	m_pOuter->UpdateOverhealEffect();
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnAddInvulnerable( void )
{
#ifndef CLIENT_DLL
	// Stock uber removes negative conditions.
	if ( InCond( TF_COND_BURNING ) )
	{
		RemoveCond( TF_COND_BURNING );
	}

	if ( InCond( TF_COND_URINE ) )
	{
		RemoveCond( TF_COND_URINE );
	}
#else
	if ( m_pOuter->IsLocalPlayer() )
	{
		char *pEffectName = NULL;

		switch( m_pOuter->GetTeamNumber() )
		{
		case TF_TEAM_BLUE:
			pEffectName = "effects/invuln_overlay_blue";
			break;
		case TF_TEAM_RED:
			pEffectName =  "effects/invuln_overlay_red";
			break;
		default:
			pEffectName = "effects/invuln_overlay_blue";
			break;
		}

		IMaterial *pMaterial = materials->FindMaterial( pEffectName, TEXTURE_GROUP_CLIENT_EFFECTS, false );
		if ( !IsErrorMaterial( pMaterial ) )
		{
			view->SetScreenOverlayMaterial( pMaterial );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveInvulnerable(void)
{
#ifdef CLIENT_DLL
	if ( m_pOuter->IsLocalPlayer() )
	{
		view->SetScreenOverlayMaterial( NULL );
	}
#endif
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayerShared::ShouldShowRecentlyTeleported( void )
{
	if ( m_pOuter->IsPlayerClass( TF_CLASS_SPY ) )
	{
		if ( InCond( TF_COND_STEALTHED ) )
			return false;

		if ( InCond( TF_COND_DISGUISED ) && ( m_pOuter->IsLocalPlayer() || !m_pOuter->InSameTeam( C_TFPlayer::GetLocalTFPlayer() ) ) )
		{
			if ( GetDisguiseTeam() != m_nTeamTeleporterUsed )
				return false;
		}
		else if ( m_pOuter->GetTeamNumber() != m_nTeamTeleporterUsed )
			return false;
	}

	return ( InCond( TF_COND_TELEPORTED ) );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnAddTeleported(void)
{
#ifdef CLIENT_DLL
	m_pOuter->UpdateRecentlyTeleportedEffect();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveTeleported(void)
{
#ifdef CLIENT_DLL
	m_pOuter->UpdateRecentlyTeleportedEffect();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnAddTaunting(void)
{
	CTFWeaponBase *pWpn = m_pOuter->GetActiveTFWeapon();
	if (pWpn)
	{
		// cancel any reload in progress.
		pWpn->AbortReload();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveTaunting(void)
{
#ifdef GAME_DLL
	m_pOuter->ClearTauntAttack();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnAddStunned(void)
{
	CTFWeaponBase *pWeapon = m_pOuter->GetActiveTFWeapon();

	if ( pWeapon )
	{
		pWeapon->OnControlStunned();
	}

	// Check if effects are disabled
	if ( !( m_nStunFlags & TF_STUNFLAG_NOSOUNDOREFFECT ) )
	{
#ifdef CLIENT_DLL
		if ( !m_pStun )
		{
			if ( m_nStunFlags & TF_STUNFLAG_BONKEFFECT )
			{
				// Half stun
				m_pStun = m_pOuter->ParticleProp()->Create( "conc_stars", PATTACH_POINT_FOLLOW, "head" );
			}
			else if ( m_nStunFlags & TF_STUNFLAG_GHOSTEFFECT )
			{
				// Ghost stun
				m_pStun = m_pOuter->ParticleProp()->Create( "yikes_fx", PATTACH_POINT_FOLLOW, "head" );
			}
		}
#else
		if ( ( m_nStunFlags & TF_STUNFLAG_GHOSTEFFECT ) )
		{
			// Play the scream sound
			m_pOuter->EmitSound( "Halloween.PlayerScream" );
		}
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveStunned(void)
{
	m_flStunExpireTime = 0.0f;
	m_flStunMovementSpeed = 0.0f;
	m_flStunResistance = 0.0f;
	m_hStunner = NULL;
	m_iStunPhase = STUN_PHASE_NONE;

	m_pOuter->TeamFortress_SetSpeed();

	CTFWeaponBase *pWeapon = m_pOuter->GetActiveTFWeapon();

	if ( pWeapon )
	{
		pWeapon->SetWeaponVisible( true );
	}

#ifdef CLIENT_DLL
	m_pOuter->ParticleProp()->StopEmission( m_pStun );
	m_pStun = NULL;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnAddSlowed(void)
{
	m_pOuter->TeamFortress_SetSpeed();
}

//-----------------------------------------------------------------------------
// Purpose: Remove slowdown effect
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveSlowed(void)
{
	// Set speed back to normal
	m_pOuter->TeamFortress_SetSpeed();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnAddCritboosted(void)
{
#ifdef CLIENT_DLL
	UpdateCritBoostEffect();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnAddHalloweenGiant(void)
{
#ifdef GAME_DLL
	m_pOuter->SetModelScale(2.0, 0.0);

	m_pOuter->SetMaxHealth(m_pOuter->GetPlayerClass()->GetMaxHealth() * 10);
	m_pOuter->SetHealth(m_pOuter->GetMaxHealth());
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveHalloweenGiant(void)
{
#ifdef GAME_DLL
	m_pOuter->SetModelScale(1.0, 0.0);

	m_pOuter->SetMaxHealth(m_pOuter->GetPlayerClass()->GetMaxHealth());
	m_pOuter->SetHealth(m_pOuter->GetMaxHealth());
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnAddHalloweenTiny(void)
{
#ifdef GAME_DLL
	m_pOuter->SetModelScale(0.5, 0.0);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveHalloweenTiny(void)
{
#ifdef GAME_DLL
	m_pOuter->SetModelScale(1.0, 0.0);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveCritboosted(void)
{
#ifdef CLIENT_DLL
	UpdateCritBoostEffect();
#endif
}

void CTFPlayerShared::OnAddSpeedBoost( void )
{
	m_pOuter->TeamFortress_SetSpeed();
	UpdateSpeedBoostEffects();
}
//-----------------------------------------------------------------------------
 // Purpose: 
 //-----------------------------------------------------------------------------
 void CTFPlayerShared::OnRemoveSpeedBoost( void )
 {
 #ifdef GAME_DLL
 	CSingleUserRecipientFilter filter( m_pOuter );
 	m_pOuter->EmitSound( filter, m_pOuter->entindex(), "PowerupSpeedBoost.WearOff" );
#endif
	m_pOuter->TeamFortress_SetSpeed();
	UpdateSpeedBoostEffects();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnAddUrine(void)
{
#ifdef GAME_DLL
	m_pOuter->SpeakConceptIfAllowed( MP_CONCEPT_JARATE_HIT );
#else
	m_pOuter->ParticleProp()->Create( "peejar_drips", PATTACH_ABSORIGIN_FOLLOW );

	if ( m_pOuter->IsLocalPlayer() )
	{
		IMaterial *pMaterial = materials->FindMaterial( "effects/jarate_overlay", TEXTURE_GROUP_CLIENT_EFFECTS, false );
		if ( !IsErrorMaterial( pMaterial ) )
		{
			view->SetScreenOverlayMaterial( pMaterial );
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveUrine(void)
{
#ifdef GAME_DLL
	if( m_nPlayerState != TF_STATE_DYING )
	{
		m_hUrineAttacker = NULL;
	}
#else
	m_pOuter->ParticleProp()->StopParticlesNamed( "peejar_drips" );

	if ( m_pOuter->IsLocalPlayer() )
	{
		view->SetScreenOverlayMaterial( NULL );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnAddPhase(void)
{
#ifdef GAME_DLL
	m_pOuter->DropFlag();
#endif
	UpdatePhaseEffects();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemovePhase(void)
{
#ifdef GAME_DLL
	m_pOuter->SpeakConceptIfAllowed( MP_CONCEPT_TIRED );

	for ( int i = 0; i < m_pPhaseTrails.Count(); i++ )
	{
		m_pPhaseTrails[i]->SUB_Remove();
	}
	m_pPhaseTrails.RemoveAll();
#else
	m_pOuter->ParticleProp()->StopEmission( m_pWarp );
	m_pWarp = NULL;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnAddBuff( void )
{
#ifdef CLIENT_DLL
	// Start the buff effect
	if ( !m_pBuffAura )
	{
		const char *pszEffectName = ConstructTeamParticle( "soldierbuff_%s_buffed", m_pOuter->GetTeamNumber() );

		m_pBuffAura = m_pOuter->ParticleProp()->Create( pszEffectName, PATTACH_ABSORIGIN_FOLLOW );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveBuff( void )
{
#ifdef CLIENT_DLL
	if ( m_pBuffAura )
	{
		m_pOuter->ParticleProp()->StopEmission( m_pBuffAura );
		m_pBuffAura = NULL;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::RecalculatePlayerBodygroups(void)
{
	//CTFWeaponBase::UpdateWeaponBodyGroups((CTFWeaponBase *)v4, 0);
	//CEconWearable::UpdateWearableBodyGroups(*((CEconWearable **)this + 521));
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::Burn( CTFPlayer *pAttacker, CTFWeaponBase *pWeapon /*= NULL*/, float flFlameDuration )
{
#ifdef CLIENT_DLL

#else
	// Don't bother igniting players who have just been killed by the fire damage.
	if ( !m_pOuter->IsAlive() )
		return;

	// pyros don't burn persistently or take persistent burning damage, but we show brief burn effect so attacker can tell they hit
	bool bVictimIsPyro = ( TF_CLASS_PYRO ==  m_pOuter->GetPlayerClass()->GetClassIndex() );

	if (!InCond(TF_COND_BURNING))
	{
		// Start burning
		AddCond(TF_COND_BURNING);
		m_flFlameBurnTime = gpGlobals->curtime;	//asap
		// let the attacker know he burned me
		if (pAttacker && !bVictimIsPyro)
		{
			pAttacker->OnBurnOther(m_pOuter);
		}
	}

	float flFlameLife = bVictimIsPyro ? TF_BURNING_FLAME_LIFE_PYRO : TF_BURNING_FLAME_LIFE;

	if ( flFlameDuration != -1.0f  )
		flFlameLife = flFlameDuration;

	CALL_ATTRIB_HOOK_FLOAT_ON_OTHER(pAttacker, flFlameLife, mult_wpn_burntime);

	m_flFlameRemoveTime = gpGlobals->curtime + flFlameLife;
	m_hBurnAttacker = pAttacker;
	m_hBurnWeapon = pWeapon;

#endif
}

void CTFPlayerShared::StunPlayer( float flDuration, float flSpeed, float flResistance, int nStunFlags, CTFPlayer *pStunner )
{
	float flNextStunExpireTime = max( m_flStunExpireTime, gpGlobals->curtime + flDuration );
	m_hStunner = pStunner;
	m_nStunFlags = nStunFlags;
	m_flStunMovementSpeed = flSpeed;
	m_flStunResistance = flResistance;

	if ( m_flStunExpireTime < flNextStunExpireTime )
	{
		AddCond( TF_COND_STUNNED );
		m_flStunExpireTime = flNextStunExpireTime;

#ifdef GAME_DLL

		// Don't play the stun sound if sounds are disabled or the stunner isn't a player
		if ( m_hStunner.Get() && !( m_nStunFlags & TF_STUNFLAG_NOSOUNDOREFFECT ) )
		{
			const char *pszStunSound = "\0";
			if ( m_nStunFlags & TF_STUNFLAG_CHEERSOUND )
			{
				// Moonshot/Grandslam
				pszStunSound = "TFPlayer.StunImpactRange";
			}
			else
			{
				// Normal stun
				pszStunSound = "TFPlayer.StunImpact";
			}
			m_pOuter->PlayStunSound( pStunner, pszStunSound );
		}
#endif
	}
}

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Purpose: Bonk phase effects
//-----------------------------------------------------------------------------
void CTFPlayerShared::AddPhaseEffects( void )
{
	CTFPlayer *pPlayer = m_pOuter;
	if ( !pPlayer)
		return;


	// TODO: Clean this up a bit more
	const char* pszEffect = m_pOuter->GetTeamNumber() == TF_TEAM_BLUE ? "effects/beam001_blu.vmt" : "effects/beam001_red.vmt";
	Vector vecOrigin = pPlayer->GetAbsOrigin();
	
	CSpriteTrail *pPhaseTrail = CSpriteTrail::SpriteTrailCreate( pszEffect, vecOrigin, true );
	pPhaseTrail->SetTransparency( kRenderTransAlpha, 255, 255, 255, 255, 0 );
	pPhaseTrail->SetStartWidth( 12.0f );
	pPhaseTrail->SetTextureResolution( 0.01416667 );
	pPhaseTrail->SetLifeTime( 1.0 );
	pPhaseTrail->SetAttachment( pPlayer, pPlayer->LookupAttachment( "back_upper" ) );
	m_pPhaseTrails.AddToTail( pPhaseTrail );

	pPhaseTrail = CSpriteTrail::SpriteTrailCreate( pszEffect, vecOrigin, true );
	pPhaseTrail->SetTransparency( kRenderTransAlpha, 255, 255, 255, 255, 0 );
	pPhaseTrail->SetStartWidth( 16.0f );
	pPhaseTrail->SetTextureResolution( 0.01416667 );
	pPhaseTrail->SetLifeTime( 1.0 );
	pPhaseTrail->SetAttachment( pPlayer, pPlayer->LookupAttachment( "back_lower" ) );
	m_pPhaseTrails.AddToTail( pPhaseTrail );

	// White trail for socks
	pPhaseTrail = CSpriteTrail::SpriteTrailCreate( "effects/beam001_white.vmt", vecOrigin, true );
	pPhaseTrail->SetTransparency( kRenderTransAlpha, 255, 255, 255, 255, 0 );
	pPhaseTrail->SetStartWidth( 8.0f );
	pPhaseTrail->SetTextureResolution( 0.01416667 );
	pPhaseTrail->SetLifeTime( 0.5 );
	pPhaseTrail->SetAttachment( pPlayer, pPlayer->LookupAttachment( "foot_R" ) );
	m_pPhaseTrails.AddToTail( pPhaseTrail );

	pPhaseTrail = CSpriteTrail::SpriteTrailCreate( "effects/beam001_white.vmt", vecOrigin, true );
	pPhaseTrail->SetTransparency( kRenderTransAlpha, 255, 255, 255, 255, 0 );
	pPhaseTrail->SetStartWidth( 8.0f );
	pPhaseTrail->SetTextureResolution( 0.01416667 );
	pPhaseTrail->SetLifeTime( 0.5 );
	pPhaseTrail->SetAttachment( pPlayer, pPlayer->LookupAttachment( "foot_L" ) );
	m_pPhaseTrails.AddToTail( pPhaseTrail );
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Update phase effects
//-----------------------------------------------------------------------------
void CTFPlayerShared::UpdatePhaseEffects(void)
{
	if ( !InCond( TF_COND_PHASE ) )
	{
		return;
	}

#ifdef CLIENT_DLL
	if(  m_pOuter->GetAbsVelocity() != vec3_origin )
	{
		// We're on the move
		if( m_pWarp )
		{
			m_pOuter->ParticleProp()->StopEmission( m_pWarp );
			m_pWarp = NULL;
		}
	}
	else
	{
		// We're not moving
		if ( !m_pWarp )
		{
			m_pWarp = m_pOuter->ParticleProp()->Create( "warp_version", PATTACH_ABSORIGIN_FOLLOW );
		}
	}
#else
	if ( m_pPhaseTrails.IsEmpty() )
	{
		AddPhaseEffects();
	}
		
	// Turn on the trails if they're not active already
	if ( m_pPhaseTrails[0] && !m_pPhaseTrails[0]->IsOn() )
	{
		for( int i = 0; i < m_pPhaseTrails.Count(); i++ )
		{
			m_pPhaseTrails[i]->TurnOn();
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Update speedboost effects
//-----------------------------------------------------------------------------
void CTFPlayerShared::UpdateSpeedBoostEffects(void)
{
	if ( !IsSpeedBoosted() )
		return;

#ifdef CLIENT_DLL
	if(  m_pOuter->GetAbsVelocity() != vec3_origin )
	{
		// We're on the move
		if ( !m_pSpeedTrails )
		{
			m_pSpeedTrails = m_pOuter->ParticleProp()->Create( "speed_boost_trail", PATTACH_ABSORIGIN_FOLLOW );
		}
	}
	else
	{
		// We're not moving
		if( m_pSpeedTrails )
		{
			m_pOuter->ParticleProp()->StopEmission( m_pSpeedTrails );
			m_pSpeedTrails = NULL;
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveBurning(void)
{
#ifdef CLIENT_DLL
	m_pOuter->StopBurningSound();

	if ( m_pOuter->m_pBurningEffect )
	{
		m_pOuter->ParticleProp()->StopEmission( m_pOuter->m_pBurningEffect );
		m_pOuter->m_pBurningEffect = NULL;
	}

	if ( m_pOuter->IsLocalPlayer() )
	{
		view->SetScreenOverlayMaterial( NULL );
	}

	m_pOuter->m_flBurnEffectStartTime = 0;
	m_pOuter->m_flBurnEffectEndTime = 0;
#else
	m_hBurnAttacker = NULL;
	m_hBurnWeapon = NULL;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnAddStealthed(void)
{
#ifdef CLIENT_DLL
	m_pOuter->EmitSound( "Player.Spy_Cloak" );
	UpdateCritBoostEffect();
	m_pOuter->UpdateOverhealEffect();
	m_pOuter->RemoveAllDecals();
	m_pOuter->UpdateRecentlyTeleportedEffect();
#else

#endif

	m_flInvisChangeCompleteTime = gpGlobals->curtime + tf_spy_invis_time.GetFloat();

	// set our offhand weapon to be the invis weapon
	int i;
	for (i = 0; i < m_pOuter->WeaponCount(); i++)
	{
		CTFWeaponBase *pWpn = (CTFWeaponBase *)m_pOuter->GetWeapon(i);
		if (!pWpn)
			continue;

		if (pWpn->GetWeaponID() != TF_WEAPON_INVIS)
			continue;

		// try to switch to this weapon
		m_pOuter->SetOffHandWeapon(pWpn);
		break;
	}

	m_pOuter->TeamFortress_SetSpeed();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveStealthed(void)
{
#ifdef CLIENT_DLL
	m_pOuter->EmitSound( "Player.Spy_UnCloak" );
	UpdateCritBoostEffect();
	m_pOuter->UpdateOverhealEffect();
	m_pOuter->UpdateRecentlyTeleportedEffect();
#endif

	m_pOuter->HolsterOffHandWeapon();

	m_pOuter->TeamFortress_SetSpeed();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveDisguising(void)
{
#ifdef CLIENT_DLL
	if ( m_pOuter->m_pDisguisingEffect )
	{
		m_pOuter->ParticleProp()->StopEmission( m_pOuter->m_pDisguisingEffect );
		m_pOuter->m_pDisguisingEffect = NULL;
	}
#else
	// PistonMiner: Removed the reset as we need this for later.

	//m_nDesiredDisguiseTeam = TF_SPY_UNDEFINED;

	// Do not reset this value, we use the last desired disguise class for the
	// 'lastdisguise' command

	//m_nDesiredDisguiseClass = TF_CLASS_UNDEFINED;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnRemoveDisguised(void)
{
#ifdef CLIENT_DLL
	if ( m_pOuter->IsEnemyPlayer() )
	{
		TFPlayerClassData_t *pData = GetPlayerClassData( TF_CLASS_SPY );
		int iIndex = modelinfo->GetModelIndex( pData->GetModelName() );

		m_pOuter->SetModelIndex( iIndex );
	}

	m_pOuter->EmitSound( "Player.Spy_Disguise" );

	// They may have called for medic and created a visible medic bubble
	m_pOuter->ParticleProp()->StopParticlesNamed( "speech_mediccall", true );

	UpdateCritBoostEffect();
	m_pOuter->UpdateOverhealEffect();

#else
	m_nDisguiseTeam = TF_SPY_UNDEFINED;
	m_nDisguiseClass.Set(TF_CLASS_UNDEFINED);
	m_nMaskClass = TF_CLASS_UNDEFINED;
	m_hDisguiseTarget.Set(NULL);
	m_iDisguiseTargetIndex = TF_DISGUISE_TARGET_INDEX_NONE;
	m_iDisguiseHealth = 0;
	m_iDisguiseMaxHealth = 0;
	m_flDisguiseChargeLevel = 0.0f;
	m_DisguiseItem.SetItemDefIndex( -1 );

	// Update the player model and skin.
	m_pOuter->UpdateModel();

	m_pOuter->TeamFortress_SetSpeed();

	m_pOuter->ClearExpression();
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnAddBurning(void)
{
#ifdef CLIENT_DLL
	// Start the burning effect
	if ( !m_pOuter->m_pBurningEffect )
	{
		const char *pszEffectName = ConstructTeamParticle( "burningplayer_%s", m_pOuter->GetTeamNumber(), true );

		m_pOuter->m_pBurningEffect = m_pOuter->ParticleProp()->Create( pszEffectName, PATTACH_ABSORIGIN_FOLLOW );

		m_pOuter->m_flBurnEffectStartTime = gpGlobals->curtime;
		m_pOuter->m_flBurnEffectEndTime = gpGlobals->curtime + TF_BURNING_FLAME_LIFE;
	}
	// set the burning screen overlay
	if ( m_pOuter->IsLocalPlayer() )
	{
		IMaterial *pMaterial = materials->FindMaterial( "effects/imcookin", TEXTURE_GROUP_CLIENT_EFFECTS, false );
		if ( !IsErrorMaterial( pMaterial ) )
		{
			view->SetScreenOverlayMaterial( pMaterial );
		}
	}
#endif

	/*
	#ifdef GAME_DLL

	if ( player == robin || player == cook )
	{
	CSingleUserRecipientFilter filter( m_pOuter );
	TFGameRules()->SendHudNotification( filter, HUD_NOTIFY_SPECIAL );
	}

	#endif
	*/

	// play a fire-starting sound
	m_pOuter->EmitSound("Fire.Engulf");
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CTFPlayerShared::GetStealthNoAttackExpireTime(void)
{
	return m_flStealthNoAttackExpire;
}

//-----------------------------------------------------------------------------
// Purpose: Sets whether this player is dominating the specified other player
//-----------------------------------------------------------------------------
void CTFPlayerShared::SetPlayerDominated(CTFPlayer *pPlayer, bool bDominated)
{
	int iPlayerIndex = pPlayer->entindex();
	m_bPlayerDominated.Set(iPlayerIndex, bDominated);
	pPlayer->m_Shared.SetPlayerDominatingMe(m_pOuter, bDominated);
}

//-----------------------------------------------------------------------------
// Purpose: Sets whether this player is being dominated by the other player
//-----------------------------------------------------------------------------
void CTFPlayerShared::SetPlayerDominatingMe(CTFPlayer *pPlayer, bool bDominated)
{
	int iPlayerIndex = pPlayer->entindex();
	m_bPlayerDominatingMe.Set(iPlayerIndex, bDominated);
}

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: Gets the number of players currently dominated
//-----------------------------------------------------------------------------
int CTFPlayerShared::GetDominationCount( void )
{
	int iDominationCount = 0;
	for (int playerIndex = 1; playerIndex <= MAX_PLAYERS; playerIndex++)
	{
		if (IsPlayerDominated(playerIndex))
		{
			iDominationCount++;
		}
	}
	return iDominationCount;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Returns whether this player is dominating the specified other player
//-----------------------------------------------------------------------------
bool CTFPlayerShared::IsPlayerDominated(int iPlayerIndex)
{
#ifdef CLIENT_DLL
	// On the client, we only have data for the local player.
	// As a result, it's only valid to ask for dominations related to the local player
	C_TFPlayer *pLocalPlayer = C_TFPlayer::GetLocalTFPlayer();
	if ( !pLocalPlayer )
		return false;

	Assert( m_pOuter->IsLocalPlayer() || pLocalPlayer->entindex() == iPlayerIndex );

	if ( m_pOuter->IsLocalPlayer() )
		return m_bPlayerDominated.Get( iPlayerIndex );

	return pLocalPlayer->m_Shared.IsPlayerDominatingMe( m_pOuter->entindex() );
#else
	// Server has all the data.
	return m_bPlayerDominated.Get( iPlayerIndex );
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFPlayerShared::IsPlayerDominatingMe( int iPlayerIndex )
{
	return m_bPlayerDominatingMe.Get( iPlayerIndex );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayerShared::NoteLastDamageTime( int nDamage )
{
	// we took damage
	if (nDamage > 5)
	{
		m_flLastStealthExposeTime = gpGlobals->curtime;
		AddCond( TF_COND_STEALTHED_BLINK );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayerShared::OnSpyTouchedByEnemy( void )
{
	m_flLastStealthExposeTime = gpGlobals->curtime;
	AddCond( TF_COND_STEALTHED_BLINK );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayerShared::FadeInvis( float flInvisFadeTime )
{
	RemoveCond( TF_COND_STEALTHED );

	if ( flInvisFadeTime < 0.15 )
	{
		// this was a force respawn, they can attack whenever
	}
	else
	{
		// next attack in some time
		m_flStealthNoAttackExpire = gpGlobals->curtime + tf_spy_cloak_no_attack_time.GetFloat();
	}

	m_flInvisChangeCompleteTime = gpGlobals->curtime + flInvisFadeTime;
}

//-----------------------------------------------------------------------------
// Purpose: Approach our desired level of invisibility
//-----------------------------------------------------------------------------
void CTFPlayerShared::InvisibilityThink( void )
{
	float flTargetInvis = 0.0f;
	float flTargetInvisScale = 1.0f;
	if ( InCond( TF_COND_STEALTHED_BLINK ) || InCond( TF_COND_URINE ) )
	{
		// We were bumped into or hit for some damage.
		flTargetInvisScale = TF_SPY_STEALTH_BLINKSCALE;/*tf_spy_stealth_blink_scale.GetFloat();*/
	}

	// Go invisible or appear.
	if ( m_flInvisChangeCompleteTime > gpGlobals->curtime )
	{
		if ( InCond( TF_COND_STEALTHED ) )
		{
			flTargetInvis = 1.0f - ( ( m_flInvisChangeCompleteTime - gpGlobals->curtime ) );
		}
		else
		{
			flTargetInvis = ( ( m_flInvisChangeCompleteTime - gpGlobals->curtime) * 0.5f );
		}
	}
	else
	{
		if ( InCond( TF_COND_STEALTHED ) )
		{
			flTargetInvis = 1.0f;
		}
		else
		{
			flTargetInvis = 0.0f;
		}
	}

	flTargetInvis *= flTargetInvisScale;
	m_flInvisibility = clamp( flTargetInvis, 0.0f, 1.0f );
}


//-----------------------------------------------------------------------------
// Purpose: How invisible is the player [0..1]
//-----------------------------------------------------------------------------
float CTFPlayerShared::GetPercentInvisible(void)
{
	return m_flInvisibility;
}

//-----------------------------------------------------------------------------
// Purpose: Start the process of disguising
//-----------------------------------------------------------------------------
void CTFPlayerShared::Disguise( int nTeam, int nClass )
{
#ifndef CLIENT_DLL
	int nRealTeam = m_pOuter->GetTeamNumber();
	int nRealClass = m_pOuter->GetPlayerClass()->GetClassIndex();

	Assert( ( nClass >= TF_CLASS_SCOUT ) && ( nClass <= TF_CLASS_ENGINEER ) );

	// we're not a spy
	if ( nRealClass != TF_CLASS_SPY )
	{
		return;
	}

	// Can't disguise while taunting.
	if ( InCond( TF_COND_TAUNTING ) || InCond( TF_COND_STUNNED ) )
	{
		return;
	}

	// we're not disguising as anything but ourselves (so reset everything)
	if ( nRealTeam == nTeam && nRealClass == nClass )
	{
		RemoveDisguise();
		return;
	}

	// Ignore disguise of the same type, switch disguise weapon instead.
	if ( nTeam == m_nDisguiseTeam && nClass == m_nDisguiseClass )
	{
		CTFWeaponBase *pWeapon = m_pOuter->GetActiveTFWeapon();
		RecalcDisguiseWeapon( pWeapon ? pWeapon->GetSlot() : 0 );
		return;
	}

	// invalid team
	if ( nTeam <= TEAM_SPECTATOR || nTeam >= TF_TEAM_COUNT )
	{
		return;
	}

	// invalid class
	if ( nClass <= TF_CLASS_UNDEFINED || nClass >= TF_CLASS_COUNT_ALL )
	{
		return;
	}

	m_nDesiredDisguiseClass = nClass;
	m_nDesiredDisguiseTeam = nTeam;

	AddCond( TF_COND_DISGUISING );

	// Start the think to complete our disguise

	// Switching disguises is faster if we're already disguised
	if ( InCond(TF_COND_DISGUISED ) )
		m_flDisguiseCompleteTime = gpGlobals->curtime + TF_TIME_TO_CHANGE_DISGUISE;
	else
		m_flDisguiseCompleteTime = gpGlobals->curtime + TF_TIME_TO_DISGUISE;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Set our target with a player we've found to emulate
//-----------------------------------------------------------------------------
#ifndef CLIENT_DLL
void CTFPlayerShared::FindDisguiseTarget(void)
{
	m_hDisguiseTarget = m_pOuter->TeamFortress_GetDisguiseTarget(m_nDisguiseTeam, m_nDisguiseClass);
	if (m_hDisguiseTarget)
	{
		m_iDisguiseTargetIndex.Set(m_hDisguiseTarget.Get()->entindex());
		Assert(m_iDisguiseTargetIndex >= 1 && m_iDisguiseTargetIndex <= MAX_PLAYERS);
	}
	else
	{
		m_iDisguiseTargetIndex.Set(TF_DISGUISE_TARGET_INDEX_NONE);
	}
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Complete our disguise
//-----------------------------------------------------------------------------
void CTFPlayerShared::CompleteDisguise(void)
{
#ifndef CLIENT_DLL
	AddCond(TF_COND_DISGUISED);

	m_nDisguiseClass = m_nDesiredDisguiseClass;
	m_nDisguiseTeam = m_nDesiredDisguiseTeam;

	RemoveCond(TF_COND_DISGUISING);

	FindDisguiseTarget();

	CTFPlayer *pDisguiseTarget = ToTFPlayer(GetDisguiseTarget());

	// If we have a disguise target with matching class then take their values.
	// Otherwise, generate random health and uber.
	if ( pDisguiseTarget && pDisguiseTarget->IsPlayerClass( m_nDisguiseClass ) )
	{
		int iMaxHealth = pDisguiseTarget->GetMaxHealth();

		if ( pDisguiseTarget->IsAlive() )
			m_iDisguiseHealth = pDisguiseTarget->GetHealth();

		else
			m_iDisguiseHealth = random->RandomInt( iMaxHealth / 2, iMaxHealth );
		m_iDisguiseMaxHealth = iMaxHealth;


		if (m_nDisguiseClass == TF_CLASS_MEDIC)
		{
			m_flDisguiseChargeLevel = pDisguiseTarget->MedicGetChargeLevel();
		}
	}

	else
	{
		int iMaxHealth = GetPlayerClassData(m_nDisguiseClass)->m_nMaxHealth;
		m_iDisguiseHealth = random->RandomInt(iMaxHealth / 2, iMaxHealth);
		m_iDisguiseMaxHealth = iMaxHealth;

		if (m_nDisguiseClass == TF_CLASS_MEDIC)
		{
			m_flDisguiseChargeLevel = random->RandomFloat(0.0f, 0.99f);
		}
	}

	if (m_nDisguiseClass == TF_CLASS_SPY)
	{
		m_nMaskClass = random->RandomInt(TF_FIRST_NORMAL_CLASS, TF_CLASS_COUNT);
	}
	// Update the player model and skin.
	m_pOuter->UpdateModel();

	m_pOuter->TeamFortress_SetSpeed();

	m_pOuter->ClearExpression();

	m_DisguiseItem.SetItemDefIndex( -1 );

	RecalcDisguiseWeapon();
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::SetDisguiseHealth(int iDisguiseHealth)
{
	m_iDisguiseHealth = iDisguiseHealth;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFPlayerShared::AddDisguiseHealth(int iHealthToAdd, bool bOverheal /*= false*/)
{
	Assert(InCond(TF_COND_DISGUISED));

	int iMaxHealth;
	if (!bOverheal)
		iMaxHealth = GetDisguiseMaxHealth();
	else
		iMaxHealth = GetDisguiseMaxBuffedHealth();

	iHealthToAdd = clamp(iHealthToAdd, 0, iMaxHealth - m_iDisguiseHealth);
	if (iHealthToAdd <= 0)
		return 0;

	m_iDisguiseHealth += iHealthToAdd;

	return iHealthToAdd;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::RemoveDisguise(void)
{
#ifdef CLIENT_DLL


#else
	RemoveCond(TF_COND_DISGUISED);
	RemoveCond(TF_COND_DISGUISING);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::RecalcDisguiseWeapon(int iSlot /*= 0*/)
{
#ifndef CLIENT_DLL
	// IMPORTANT!!! - This whole function will need to be rewritten if we switch to using item schema.
	// So please remind me about this when we do. (Nicknine)
	if ( !InCond( TF_COND_DISGUISED ) )
	{
		m_DisguiseItem.SetItemDefIndex( -1 );
		return;
	}

	Assert(m_pOuter->GetPlayerClass()->GetClassIndex() == TF_CLASS_SPY);

	CEconItemView *pDisguiseItem = NULL;
	CTFPlayer *pDisguiseTarget = ToTFPlayer(GetDisguiseTarget());

	// Find the weapon in the same slot
	for ( int i = 0; i < TF_PLAYER_WEAPON_COUNT; i++ )	
	{
		// Use disguise target's weapons if possible.
		CEconItemView *pItem = NULL;

		if ( pDisguiseTarget && pDisguiseTarget->IsPlayerClass( m_nDisguiseClass ) )
			pItem = pDisguiseTarget->GetLoadoutItem( m_nDisguiseClass, i );
		else
			pItem = GetTFInventory()->GetItem( m_nDisguiseClass, i, 0 );

		if ( !pItem )
			continue;

			CTFWeaponInfo *pWeaponInfo = GetTFWeaponInfoForItem( pItem->GetItemDefIndex(), m_nDisguiseClass );
		if ( pWeaponInfo && pWeaponInfo->iSlot == iSlot )
			{
				pDisguiseItem = pItem;
				break;
			}
		}

	if ( iSlot == 0 )
	{
		AssertMsg( pDisguiseItem, "Cannot find primary disguise weapon for desired disguise class %d\n", m_nDisguiseClass );
	}

	// Don't switch to builder as it's too complicated.
	if ( pDisguiseItem )
	{
		m_DisguiseItem = *pDisguiseItem;
	}
#else
	if ( !InCond( TF_COND_DISGUISED ) )
	{
		m_iDisguiseWeaponModelIndex = -1;
		m_pDisguiseWeaponInfo = NULL;
		return;
	}

	CTFWeaponInfo *pDisguiseWeaponInfo = GetTFWeaponInfoForItem(m_DisguiseItem.GetItemDefIndex(), m_nDisguiseClass);

	m_pDisguiseWeaponInfo = pDisguiseWeaponInfo;
	m_iDisguiseWeaponModelIndex = -1;

	if ( pDisguiseWeaponInfo )
	{
		m_iDisguiseWeaponModelIndex = modelinfo->GetModelIndex(m_DisguiseItem.GetWorldDisplayModel());
	}
#endif
}

#ifdef CLIENT_DLL
//-----------------------------------------------------------------------------
// Purpose: Crit effects handling.
//-----------------------------------------------------------------------------
void CTFPlayerShared::UpdateCritBoostEffect( bool bForceHide /*= false*/ )
{
	bool bShouldShow = !bForceHide;

	if ( bShouldShow )
	{
		if ( m_pOuter->IsDormant() )
		{
			bShouldShow = false;
		}
		else if ( !IsCritBoosted() )
		{
			bShouldShow = false;
		}
		else if ( InCond( TF_COND_STEALTHED ) )
		{
			bShouldShow = false;
		}
		else if ( InCond( TF_COND_DISGUISED ) &&
			!m_pOuter->InSameTeam( C_TFPlayer::GetLocalTFPlayer() ) &&
			m_pOuter->GetTeamNumber() != GetDisguiseTeam() )
		{
			// Don't show crit effect for disguised enemy spies unless they're disguised
			// as their own team.
			bShouldShow = false;
		}
	}

	if ( bShouldShow )
	{
		// Update crit effect model.
		if ( m_pCritEffect )
		{
			if ( m_hCritEffectHost.Get() )
				m_hCritEffectHost->ParticleProp()->StopEmission( m_pCritEffect );
			m_pCritEffect = NULL;
		}

		if ( !m_pOuter->ShouldDrawThisPlayer() )
		{
			m_hCritEffectHost = m_pOuter->GetViewModel();
		}
		else
		{
			C_BaseCombatWeapon *pWeapon = m_pOuter->GetActiveWeapon();
			
			// Don't add crit effect to weapons without a model.
			if (pWeapon && pWeapon->GetWorldModelIndex() != 0)
			{
				m_hCritEffectHost = pWeapon;
			}
			else
			{
				m_hCritEffectHost = m_pOuter;
			}
		}

		if ( m_hCritEffectHost.Get() )
		{
			const char *pszEffect = ConstructTeamParticle( "critgun_weaponmodel_%s", m_pOuter->GetTeamNumber(), true, g_aTeamNamesShort );
			m_pCritEffect = m_hCritEffectHost->ParticleProp()->Create( pszEffect, PATTACH_ABSORIGIN_FOLLOW );
		}

		if ( !m_pCritSound )
		{
			CSoundEnvelopeController &controller = CSoundEnvelopeController::GetController();
			CLocalPlayerFilter filter;
			m_pCritSound = controller.SoundCreate( filter, m_pOuter->entindex(), "Weapon_General.CritPower" );
			controller.Play( m_pCritSound, 1.0, 100 );
		}
	}
	else
	{
		if ( m_pCritEffect )
		{
			if ( m_hCritEffectHost.Get() )
				m_hCritEffectHost->ParticleProp()->StopEmission( m_pCritEffect );
			m_pCritEffect = NULL;
		}

		m_hCritEffectHost = NULL;

		if ( m_pCritSound )
		{
			CSoundEnvelopeController::GetController().SoundDestroy( m_pCritSound );
			m_pCritSound = NULL;
		}
	}
}

CTFWeaponInfo *CTFPlayerShared::GetDisguiseWeaponInfo( void )
{
	if (InCond(TF_COND_DISGUISED) && m_pDisguiseWeaponInfo == NULL)
	{
		RecalcDisguiseWeapon();
	}

	return m_pDisguiseWeaponInfo;
}
#endif

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Purpose: Heal players.
// pPlayer is person who healed us
//-----------------------------------------------------------------------------
void CTFPlayerShared::Heal(CTFPlayer *pPlayer, float flAmount, bool bDispenserHeal /* = false */)
{
	Assert(FindHealerIndex(pPlayer) == m_aHealers.InvalidIndex());

	healers_t newHealer;
	newHealer.pPlayer = pPlayer;
	newHealer.flAmount = flAmount;
	newHealer.bDispenserHeal = bDispenserHeal;
	m_aHealers.AddToTail(newHealer);

	AddCond(TF_COND_HEALTH_BUFF, PERMANENT_CONDITION);

	RecalculateChargeEffects();

	m_nNumHealers = m_aHealers.Count();
}

//-----------------------------------------------------------------------------
// Purpose: Heal players.
// pPlayer is person who healed us
//-----------------------------------------------------------------------------
void CTFPlayerShared::StopHealing(CTFPlayer *pPlayer)
{
	int iIndex = FindHealerIndex(pPlayer);
	Assert(iIndex != m_aHealers.InvalidIndex());

	m_aHealers.Remove(iIndex);

	if (!m_aHealers.Count())
	{
		RemoveCond(TF_COND_HEALTH_BUFF);
	}

	RecalculateChargeEffects();

	m_nNumHealers = m_aHealers.Count();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
medigun_charge_types CTFPlayerShared::GetChargeEffectBeingProvided( CTFPlayer *pPlayer )
{
	if ( !pPlayer->IsPlayerClass( TF_CLASS_MEDIC ) && !tf2c_random_weapons.GetBool() )
		return TF_CHARGE_NONE;

	CTFWeaponBase *pWpn = pPlayer->GetActiveTFWeapon();
	if ( !pWpn )
		return TF_CHARGE_NONE;

	CWeaponMedigun *pMedigun = dynamic_cast < CWeaponMedigun * >( pWpn );
	if ( pMedigun && pMedigun->IsReleasingCharge() )
		return pMedigun->GetChargeType();

	return TF_CHARGE_NONE;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::RecalculateChargeEffects(bool bInstantRemove)
{
	bool bShouldCharge[TF_CHARGE_COUNT] = {};
	CTFPlayer *pProviders[TF_CHARGE_COUNT] = {};

	if (m_pOuter->m_flPowerPlayTime > gpGlobals->curtime)
	{
		bShouldCharge[TF_CHARGE_INVULNERABLE] = true;
	}

	medigun_charge_types selfCharge = GetChargeEffectBeingProvided(m_pOuter);

	// Charging self?
	if (selfCharge != TF_CHARGE_NONE)
	{
		bShouldCharge[selfCharge] = true;
		pProviders[selfCharge] = m_pOuter;
	}
	else
	{
		// Check players healing us.
		for (int i = 0; i < m_aHealers.Count(); i++)
		{
			if (!m_aHealers[i].pPlayer)
				continue;

			CTFPlayer *pPlayer = ToTFPlayer(m_aHealers[i].pPlayer);
			if (!pPlayer)
				continue;

			medigun_charge_types chargeType = GetChargeEffectBeingProvided(pPlayer);

			if (chargeType != TF_CHARGE_NONE)
			{
				bShouldCharge[chargeType] = true;
				pProviders[chargeType] = pPlayer;
			}
		}
	}

	// Deny stock uber while carrying flag.
	if (m_pOuter->HasTheFlag())
	{
		bShouldCharge[TF_CHARGE_INVULNERABLE] = false;
	}

	for (int i = 0; i < TF_CHARGE_COUNT; i++)
	{
		float flRemoveTime = i == TF_CHARGE_INVULNERABLE ? tf_invuln_time.GetFloat() : 0.0f;
		SetChargeEffect((medigun_charge_types)i, bShouldCharge[i], bInstantRemove, g_MedigunEffects[i], flRemoveTime, pProviders[i]);
	}
}

void CTFPlayerShared::SetChargeEffect(medigun_charge_types chargeType, bool bShouldCharge, bool bInstantRemove, const MedigunEffects_t &chargeEffect, float flRemoveTime, CTFPlayer *pProvider)
{
	if (InCond(chargeEffect.condition_enable) == bShouldCharge)
	{
		if (bShouldCharge && m_flChargeOffTime[chargeType] != 0.0f)
		{
			m_flChargeOffTime[chargeType] = 0.0f;

			if (chargeEffect.condition_disable != TF_COND_LAST)
				RemoveCond(chargeEffect.condition_disable);
		}
		return;
	}

	if (bShouldCharge)
	{
		Assert(chargeType != TF_CHARGE_INVULNERABLE || !m_pOuter->HasTheFlag());

		if (m_flChargeOffTime[chargeType] != 0.0f)
		{
			m_pOuter->StopSound(chargeEffect.sound_disable);

			m_flChargeOffTime[chargeType] = 0.0f;

			if (chargeEffect.condition_disable != TF_COND_LAST)
				RemoveCond(chargeEffect.condition_disable);
		}

		// Charge on.
		AddCond(chargeEffect.condition_enable);

		CSingleUserRecipientFilter filter(m_pOuter);
		m_pOuter->EmitSound(filter, m_pOuter->entindex(), chargeEffect.sound_enable);
		m_bChargeSounds[chargeType] = true;
	}
	else
	{
		if (m_bChargeSounds[chargeType])
		{
			m_pOuter->StopSound(chargeEffect.sound_enable);
			m_bChargeSounds[chargeType] = false;
		}

		if (m_flChargeOffTime[chargeType] == 0.0f)
		{
			CSingleUserRecipientFilter filter(m_pOuter);
			m_pOuter->EmitSound(filter, m_pOuter->entindex(), chargeEffect.sound_disable);
		}

		if (bInstantRemove)
		{
			m_flChargeOffTime[chargeType] = 0.0f;
			RemoveCond(chargeEffect.condition_enable);

			if (chargeEffect.condition_disable != TF_COND_LAST)
				RemoveCond(chargeEffect.condition_disable);
		}
		else
		{
			// Already turning it off?
			if (m_flChargeOffTime[chargeType] != 0.0f)
				return;

			if (chargeEffect.condition_disable != TF_COND_LAST)
				AddCond(chargeEffect.condition_disable);

			m_flChargeOffTime[chargeType] = gpGlobals->curtime + flRemoveTime;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::TestAndExpireChargeEffect(medigun_charge_types chargeType)
{
	if (InCond(g_MedigunEffects[chargeType].condition_enable))
	{
		bool bRemoveCharge = false;

		if ((TFGameRules()->State_Get() == GR_STATE_TEAM_WIN) && (TFGameRules()->GetWinningTeam() != m_pOuter->GetTeamNumber()))
		{
			bRemoveCharge = true;
		}

		if (m_flChargeOffTime[chargeType] != 0.0f)
		{
			if (gpGlobals->curtime > m_flChargeOffTime[chargeType])
			{
				bRemoveCharge = true;
			}
		}

		if (bRemoveCharge == true)
		{
			m_flChargeOffTime[chargeType] = 0.0f;

			if (g_MedigunEffects[chargeType].condition_disable != TF_COND_LAST)
			{
				RemoveCond(g_MedigunEffects[chargeType].condition_disable);
			}

			RemoveCond(g_MedigunEffects[chargeType].condition_enable);
		}
	}
	else if (m_bChargeSounds[chargeType])
	{
		// If we're still playing charge sound but not actually charged, stop the sound.
		// This can happen if player respawns while crit boosted.
		m_pOuter->StopSound(g_MedigunEffects[chargeType].sound_enable);
		m_bChargeSounds[chargeType] = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CTFPlayerShared::FindHealerIndex(CTFPlayer *pPlayer)
{
	for (int i = 0; i < m_aHealers.Count(); i++)
	{
		if (m_aHealers[i].pPlayer == pPlayer)
			return i;
	}

	return m_aHealers.InvalidIndex();
}

//-----------------------------------------------------------------------------
// Purpose: Returns the first healer in the healer array.  Note that this
//		is an arbitrary healer.
//-----------------------------------------------------------------------------
EHANDLE CTFPlayerShared::GetFirstHealer()
{
	if (m_aHealers.Count() > 0)
		return m_aHealers.Head().pPlayer;

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::HealthKitPickupEffects(int iAmount)
{
	/*int v4; // eax@2
	int v5; // edi@8
	int v6; // edx@9
	int v7; // eax@9

	if (CTFPlayerShared::InCond((int)this, 22))
	CTFPlayerShared::RemoveCond((int)this, 22, a1, a2);
	LOBYTE(v4) = CTFPlayerShared::InCond((int)this, 25);
	if ((_BYTE)v4)
	LOBYTE(v4) = CTFPlayerShared::RemoveCond((int)this, 25, a1, a2);
	if ( iAmount )
	{
	LOBYTE(v4) = CTFPlayerShared::IsStealthed(this);
	if (!(_BYTE)v4)
	{
	v4 = *((_DWORD *)this + 521);
	if (v4)
	{
	IGameEvent *event = gameeventmanager->CreateEvent("player_healonhit");
	v5 = v4;
	if ( event )
	{
	event->SetInt( "amount", iAmount );
	event->SetInt( "entindex", m_pOuter->entindex() );
	gameeventmanager->FireEvent(event);


	(*(void(__cdecl **)(int, _DWORD, int))(*(_DWORD *)v4 + 44))(v4, "amount", iAmount);
	v6 = *(_DWORD *)(*((_DWORD *)this + 521) + 32);
	v7 = 0;
	if (v6)
	v7 = *(_WORD *)(v6 + 6);
	(*(void(__cdecl **)(int, _DWORD, int))(*(_DWORD *)v5 + 44))(v5, "entindex", v7);
	LOBYTE(v4) = (*(int(__cdecl **)(CGameRulesProxy *, int, _DWORD))(*(_DWORD *)gameeventmanager + 32))(
	gameeventmanager,
	v5,
	0);
	}
	}
	}
	}
	return v4;*/
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFWeaponBase *CTFPlayerShared::GetActiveTFWeapon() const
{
	return m_pOuter->GetActiveTFWeapon();
}

//-----------------------------------------------------------------------------
// Purpose: Team check.
//-----------------------------------------------------------------------------
bool CTFPlayerShared::IsAlly(CBaseEntity *pEntity)
{
	return (pEntity->GetTeamNumber() == m_pOuter->GetTeamNumber());
}

//-----------------------------------------------------------------------------
// Purpose: Used to determine if player should do loser animations.
//-----------------------------------------------------------------------------
bool CTFPlayerShared::IsLoser( void )
{
	if ( !m_pOuter->IsAlive() )
		return false;

	if ( tf_always_loser.GetBool() )
		return true;

	if ( TFGameRules() && TFGameRules()->State_Get() == GR_STATE_TEAM_WIN )
	{
		int iWinner = TFGameRules()->GetWinningTeam();
		if ( iWinner != m_pOuter->GetTeamNumber() )
		{
			if ( m_pOuter->IsPlayerClass( TF_CLASS_SPY ) )
			{
				if ( InCond(TF_COND_DISGUISED ) )
				{
					return (iWinner != GetDisguiseTeam());
				}
			}
			return true;
		}
	}

	// Check if we should be in the loser state while stunned
	if ( InCond( TF_COND_STUNNED ) && ( m_nStunFlags & TF_STUNFLAG_THIRDPERSON ) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFPlayerShared::GetDesiredPlayerClassIndex(void)
{
	return m_iDesiredPlayerClass;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :
// Output : Successful
//-----------------------------------------------------------------------------
bool CTFPlayerShared::AddToSpyCloakMeter( float amt, bool bForce, bool bIgnoreAttribs )
{
	CTFWeaponInvis *pInvis = dynamic_cast<CTFWeaponInvis *>( m_pOuter->Weapon_OwnsThisID( TF_WEAPON_INVIS ) );
	if (!pInvis)
		return false;

	if (!bForce && pInvis->HasMotionCloak())
		return false;

	if (pInvis->HasFeignDeath())
		amt = Min( amt, 35.0f );

	if (bIgnoreAttribs)
	{
		if (amt <= 0.0f || m_flCloakMeter >= 100.0f)
			return false;

		m_flCloakMeter = Min( Max( m_flCloakMeter + amt, 0.0f ), 100.0f );
		return true;
	}

	int iNoRegenFromItems = 0;
	CALL_ATTRIB_HOOK_INT_ON_OTHER( pInvis, iNoRegenFromItems, mod_cloak_no_regen_from_items );
	if (iNoRegenFromItems)
		return false;

	int iNoCloakWhenCloaked = 0;
	CALL_ATTRIB_HOOK_INT_ON_OTHER( pInvis, iNoCloakWhenCloaked, NoCloakWhenCloaked );
	if (iNoCloakWhenCloaked)
	{
		if (InCond( TF_COND_STEALTHED ))
			return false;
	}

	CALL_ATTRIB_HOOK_FLOAT_ON_OTHER( pInvis, amt, ReducedCloakFromAmmo );
	if (amt <= 0.0f || m_flCloakMeter >= 100.0f)
		return false;

	m_flCloakMeter = Min( Max( m_flCloakMeter + amt, 0.0f ), 100.0f );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::SetJumping(bool bJumping)
{
	m_bJumping = bJumping;
}

void CTFPlayerShared::SetAirDash(bool bAirDash)
{
	m_bAirDash = bAirDash;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::IncrementAirDucks(void)
{
	m_nAirDucked++;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::ResetAirDucks(void)
{
	m_nAirDucked = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFPlayerShared::GetSequenceForDeath( CBaseAnimating *pAnim, int iDamageCustom )
{
	const char *pszSequence = NULL;

	switch( iDamageCustom )
	{
	case TF_DMG_CUSTOM_BACKSTAB:
		pszSequence = "primary_death_backstab";
		break;
	case TF_DMG_CUSTOM_HEADSHOT:
		pszSequence = "primary_death_headshot";
		break;

	// Unused in live TF2
	/*case TF_DMG_CUSTOM_BURNING:
		pszSequence = "primary_death_burning";
		break;*/
	}

	if ( pszSequence != NULL )
	{
		return pAnim->LookupSequence( pszSequence );
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CTFPlayerShared::GetCritMult(void)
{
	float flRemapCritMul = RemapValClamped(m_iCritMult, 0, 255, 1.0, TF_DAMAGE_CRITMOD_MAXMULT);
	/*#ifdef CLIENT_DLL
	Msg("CLIENT: Crit mult %.2f - %d\n",flRemapCritMul, m_iCritMult);
	#else
	Msg("SERVER: Crit mult %.2f - %d\n", flRemapCritMul, m_iCritMult );
	#endif*/

	return flRemapCritMul;
}

//-----------------------------------------------------------------------------
// Purpose: Update rage buffs
//-----------------------------------------------------------------------------
void CTFPlayerShared::UpdateRageBuffsAndRage( void )
{
	if ( IsRageActive() )
	{
		if ( m_flEffectBarProgress > 0.0f )
		{
			if ( gpGlobals->curtime > m_flNextRageCheckTime && m_flRageTimeRemaining > 0.0f )
			{
				m_flNextRageCheckTime = gpGlobals->curtime + 1.0f;
				m_flRageTimeRemaining--;
				PulseRageBuff();
			}

			m_flEffectBarProgress -= ( 100.0f / tf_soldier_buff_pulses.GetFloat() ) * gpGlobals->frametime;
		}
		else
		{
			ResetRageSystem();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Set rage meter progress
//-----------------------------------------------------------------------------
void CTFPlayerShared::SetRageMeter( float flRagePercent, int iBuffType )
{
	if ( !IsRageActive() )
	{
		if ( m_pOuter )
		{
			CTFBuffItem *pBuffItem = ( CTFBuffItem * )m_pOuter->Weapon_GetSlot( TF_LOADOUT_SLOT_SECONDARY );
			if ( pBuffItem && pBuffItem->GetBuffType() == iBuffType )
			{
				// Only build rage if we're using this type of banner
				m_flEffectBarProgress = min( m_flEffectBarProgress + flRagePercent, 100.0f );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Activate rage
//-----------------------------------------------------------------------------
void CTFPlayerShared::ActivateRageBuff( CBaseEntity *pEntity, int iBuffType )
{
	if ( m_flEffectBarProgress < 100.0f )
		return;

	m_flNextRageCheckTime = gpGlobals->curtime + 1.0f;
	m_flRageTimeRemaining = tf_soldier_buff_pulses.GetFloat();
	m_iActiveBuffType = iBuffType;

#ifdef GAME_DLL
	//*(this + 112) = pEntity;

	if ( m_pOuter )
	{
		if ( iBuffType == TF_BUFF_OFFENSE )
		{
			m_pOuter->SpeakConceptIfAllowed( MP_CONCEPT_PLAYER_BATTLECRY );
		}
		else if ( iBuffType == TF_BUFF_DEFENSE )
		{
			m_pOuter->SpeakConceptIfAllowed( MP_CONCEPT_PLAYER_INCOMING );
		}
	}

#endif

	if ( !IsRageActive() )
	{
		m_bRageActive = true;
	}

	PulseRageBuff();
}

//-----------------------------------------------------------------------------
// Purpose: Give rage buffs to nearby players
//-----------------------------------------------------------------------------
void CTFPlayerShared::PulseRageBuff( /*CTFPlayerShared::ERageBuffSlot*/ )
{
	// g_SoldierBuffAttributeIDToConditionMap is called here in Live TF2
#ifdef GAME_DLL
	CTFPlayer *pOuter = m_pOuter;

	if ( !m_pOuter )
		return;

	CBaseEntity *pEntity = NULL;
	Vector vecOrigin = pOuter->GetAbsOrigin();

	for ( CEntitySphereQuery sphere( vecOrigin, TF_BUFF_RADIUS ); ( pEntity = sphere.GetCurrentEntity() ) != NULL; sphere.NextEntity() )
	{
		if ( !pEntity )
			continue;

		Vector vecHitPoint;
		pEntity->CollisionProp()->CalcNearestPoint( vecOrigin, &vecHitPoint );
		Vector vecDir = vecHitPoint - vecOrigin;

		CTFPlayer *pPlayer = ToTFPlayer( pEntity );

		if ( vecDir.LengthSqr() < ( TF_BUFF_RADIUS * TF_BUFF_RADIUS ) )
		{
			if ( pPlayer && pPlayer->InSameTeam( pOuter ) )
			{
				switch ( m_iActiveBuffType )
				{
				case TF_BUFF_OFFENSE:
					pPlayer->m_Shared.AddCond( TF_COND_OFFENSEBUFF, 1.2f );
					break;
				case TF_BUFF_DEFENSE:
					pPlayer->m_Shared.AddCond( TF_COND_DEFENSEBUFF, 1.2f );
					break;
				case TF_BUFF_REGENONDAMAGE:
					pPlayer->m_Shared.AddCond( TF_COND_REGENONDAMAGEBUFF, 1.2f );
					break;
				}

				// Achievements
				IGameEvent *event = gameeventmanager->CreateEvent( "player_buff" );
				if ( event )
				{
					event->SetInt( "userid", pPlayer->GetUserID() );
					event->SetInt( "buff_type", m_iActiveBuffType );
					event->SetInt( "buff_owner", pOuter->entindex() );
 					gameeventmanager->FireEvent( event );
				}
			}
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Reset rage variables
//-----------------------------------------------------------------------------
void CTFPlayerShared::ResetRageSystem( void )
{
	m_flEffectBarProgress = 0.0f;
	m_flNextRageCheckTime = 0.0f;
	m_flRageTimeRemaining = 0.0f;
	m_iActiveBuffType = 0;
	m_bRageActive = false;
}

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::UpdateCritMult(void)
{
	const float flMinMult = 1.0;
	const float flMaxMult = TF_DAMAGE_CRITMOD_MAXMULT;

	if (m_DamageEvents.Count() == 0)
	{
		m_iCritMult = RemapValClamped(flMinMult, flMinMult, flMaxMult, 0, 255);
		return;
	}

	//Msg( "Crit mult update for %s\n", m_pOuter->GetPlayerName() );
	//Msg( "   Entries: %d\n", m_DamageEvents.Count() );

	// Go through the damage multipliers and remove expired ones, while summing damage of the others
	float flTotalDamage = 0;
	for (int i = m_DamageEvents.Count() - 1; i >= 0; i--)
	{
		float flDelta = gpGlobals->curtime - m_DamageEvents[i].flTime;
		if (flDelta > tf_damage_events_track_for.GetFloat())
		{
			//Msg( "      Discarded (%d: time %.2f, now %.2f)\n", i, m_DamageEvents[i].flTime, gpGlobals->curtime );
			m_DamageEvents.Remove(i);
			continue;
		}

		// Ignore damage we've just done. We do this so that we have time to get those damage events
		// to the client in time for using them in prediction in this code.
		if (flDelta < TF_DAMAGE_CRITMOD_MINTIME)
		{
			//Msg( "      Ignored (%d: time %.2f, now %.2f)\n", i, m_DamageEvents[i].flTime, gpGlobals->curtime );
			continue;
		}

		if (flDelta > TF_DAMAGE_CRITMOD_MAXTIME)
			continue;

		//Msg( "      Added %.2f (%d: time %.2f, now %.2f)\n", m_DamageEvents[i].flDamage, i, m_DamageEvents[i].flTime, gpGlobals->curtime );

		flTotalDamage += m_DamageEvents[i].flDamage;
	}

	float flMult = RemapValClamped(flTotalDamage, 0, TF_DAMAGE_CRITMOD_DAMAGE, flMinMult, flMaxMult);

	//Msg( "   TotalDamage: %.2f   -> Mult %.2f\n", flTotalDamage, flMult );

	m_iCritMult = (int)RemapValClamped(flMult, flMinMult, flMaxMult, 0, 255);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayerShared::RecordDamageEvent(const CTakeDamageInfo &info, bool bKill)
{
	if (m_DamageEvents.Count() >= MAX_DAMAGE_EVENTS)
	{
		// Remove the oldest event
		m_DamageEvents.Remove(m_DamageEvents.Count() - 1);
	}

	int iIndex = m_DamageEvents.AddToTail();
	m_DamageEvents[iIndex].flDamage = info.GetDamage();
	m_DamageEvents[iIndex].flTime = gpGlobals->curtime;
	m_DamageEvents[iIndex].bKill = bKill;

	// Don't count critical damage
	if (info.GetDamageType() & DMG_CRITICAL)
	{
		m_DamageEvents[iIndex].flDamage /= TF_DAMAGE_CRIT_MULTIPLIER;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CTFPlayerShared::GetNumKillsInTime(float flTime)
{
	if (tf_damage_events_track_for.GetFloat() < flTime)
	{
		Warning("Player asking for damage events for time %.0f, but tf_damage_events_track_for is only tracking events for %.0f\n", flTime, tf_damage_events_track_for.GetFloat());
	}

	int iKills = 0;
	for (int i = m_DamageEvents.Count() - 1; i >= 0; i--)
	{
		float flDelta = gpGlobals->curtime - m_DamageEvents[i].flTime;
		if (flDelta < flTime)
		{
			if (m_DamageEvents[i].bKill)
			{
				iKills++;
			}
		}
	}

	return iKills;
}

#endif

//=============================================================================
//
// Shared player code that isn't CTFPlayerShared
//

//-----------------------------------------------------------------------------
// Purpose:
//   Input: info
//          bDoEffects - effects (blood, etc.) should only happen client-side.
//-----------------------------------------------------------------------------
void CTFPlayer::FireBullet(const FireBulletsInfo_t &info, bool bDoEffects, int nDamageType, int nCustomDamageType /*= TF_DMG_CUSTOM_NONE*/)
{
	// Fire a bullet (ignoring the shooter).
	Vector vecStart = info.m_vecSrc;
	Vector vecEnd = vecStart + info.m_vecDirShooting * info.m_flDistance;
	trace_t trace;
	UTIL_TraceLine(vecStart, vecEnd, (MASK_SOLID | CONTENTS_HITBOX), this, COLLISION_GROUP_NONE, &trace);

#ifdef GAME_DLL
	if (tf_debug_bullets.GetBool())
	{
		NDebugOverlay::Line(vecStart, trace.endpos, 0, 255, 0, true, 30);
	}
#endif


#ifdef CLIENT_DLL
	if (sv_showimpacts.GetInt() == 1 || sv_showimpacts.GetInt() == 2)
	{
		// draw red client impact markers
		debugoverlay->AddBoxOverlay(trace.endpos, Vector(-2, -2, -2), Vector(2, 2, 2), QAngle(0, 0, 0), 255, 0, 0, 127, 4);

		if (trace.m_pEnt && trace.m_pEnt->IsPlayer())
		{
			C_BasePlayer *player = ToBasePlayer(trace.m_pEnt);
			player->DrawClientHitboxes(4, true);
		}
	}
#else
	if (sv_showimpacts.GetInt() == 1 || sv_showimpacts.GetInt() == 3)
	{
		// draw blue server impact markers
		NDebugOverlay::Box(trace.endpos, Vector(-2, -2, -2), Vector(2, 2, 2), 0, 0, 255, 127, 4);

		if (trace.m_pEnt && trace.m_pEnt->IsPlayer())
		{
			CBasePlayer *player = ToBasePlayer(trace.m_pEnt);
			player->DrawServerHitboxes(4, true);
		}
	}
#endif

	if (sv_showplayerhitboxes.GetInt() > 0)
	{
		CBasePlayer *lagPlayer = UTIL_PlayerByIndex(sv_showplayerhitboxes.GetInt());
		if (lagPlayer)
		{
#ifdef CLIENT_DLL
			lagPlayer->DrawClientHitboxes(4, true);
#else
			lagPlayer->DrawServerHitboxes(4, true);
#endif
		}
	}

	if (trace.fraction < 1.0)
	{
		// Verify we have an entity at the point of impact.
		Assert(trace.m_pEnt);

		if (bDoEffects)
		{
			// If shot starts out of water and ends in water
			if (!(enginetrace->GetPointContents(trace.startpos) & (CONTENTS_WATER | CONTENTS_SLIME)) &&
				(enginetrace->GetPointContents(trace.endpos) & (CONTENTS_WATER | CONTENTS_SLIME)))
			{
				// Water impact effects.
				ImpactWaterTrace(trace, vecStart);
			}
			else
			{
				// Regular impact effects.

				// don't decal your teammates or objects on your team
				if (trace.m_pEnt->GetTeamNumber() != GetTeamNumber())
				{
					UTIL_ImpactTrace(&trace, nDamageType);
				}
			}

#ifdef CLIENT_DLL
			static int	tracerCount;
			if ((info.m_iTracerFreq != 0) && (tracerCount++ % info.m_iTracerFreq) == 0)
			{
				// if this is a local player, start at attachment on view model
				// else start on attachment on weapon model

				int iEntIndex = entindex();
				int iUseAttachment = TRACER_DONT_USE_ATTACHMENT;
				int iAttachment = 1;

				C_BaseCombatWeapon *pWeapon = GetActiveWeapon();

				if (pWeapon)
				{
					C_TFWeaponBase *pTFWeapon = dynamic_cast< C_TFWeaponBase* >( pWeapon );
					if ( pTFWeapon )
					{
						C_BaseViewModel *vm = pTFWeapon->GetViewmodelAddon();
						iAttachment = vm ? vm->LookupAttachment( "muzzle" ) : pWeapon->LookupAttachment( "muzzle" );
					}
					else
					{
						iAttachment = pWeapon->LookupAttachment( "muzzle" );
					}
				}

				bool bInToolRecordingMode = clienttools->IsInRecordingMode();

				// try to align tracers to actual weapon barrel if possible
				if (!ShouldDrawThisPlayer() && !bInToolRecordingMode)
				{
					C_TFViewModel *pViewModel = dynamic_cast<C_TFViewModel *>(GetViewModel());

					if (pViewModel)
					{
						iEntIndex = pViewModel->entindex();
						pViewModel->GetAttachment(iAttachment, vecStart);
					}
				}
				else if (!IsDormant())
				{
					// fill in with third person weapon model index
					C_BaseCombatWeapon *pWeapon = GetActiveWeapon();

					if (pWeapon)
					{
						iEntIndex = pWeapon->entindex();

						int nModelIndex = pWeapon->GetModelIndex();
						int nWorldModelIndex = pWeapon->GetWorldModelIndex();
						if (bInToolRecordingMode && nModelIndex != nWorldModelIndex)
						{
							pWeapon->SetModelIndex(nWorldModelIndex);
						}

						pWeapon->GetAttachment(iAttachment, vecStart);

						if (bInToolRecordingMode && nModelIndex != nWorldModelIndex)
						{
							pWeapon->SetModelIndex(nModelIndex);
						}
					}
				}

				if (tf_useparticletracers.GetBool())
				{
					const char *pszTracerEffect = GetTracerType();
					if (pszTracerEffect && pszTracerEffect[0])
					{
						char szTracerEffect[128];
						if (nDamageType & DMG_CRITICAL)
						{
							Q_snprintf(szTracerEffect, sizeof(szTracerEffect), "%s_crit", pszTracerEffect);
							pszTracerEffect = szTracerEffect;
						}

						UTIL_ParticleTracer(pszTracerEffect, vecStart, trace.endpos, entindex(), iUseAttachment, true);
					}
				}
				else
				{
					UTIL_Tracer(vecStart, trace.endpos, entindex(), iUseAttachment, 5000, true, GetTracerType());
				}
			}
#endif

		}

		// Server specific.
#ifndef CLIENT_DLL
		// See what material we hit.
		CTakeDamageInfo dmgInfo( this, info.m_pAttacker, GetActiveWeapon(), info.m_flDamage, nDamageType, nCustomDamageType );
		CalculateBulletDamageForce(&dmgInfo, info.m_iAmmoType, info.m_vecDirShooting, trace.endpos, 1.0);	//MATTTODO bullet forces
		trace.m_pEnt->DispatchTraceAttack(dmgInfo, info.m_vecDirShooting, &trace);
#endif
	}
}

#ifdef CLIENT_DLL
static ConVar tf_impactwatertimeenable("tf_impactwatertimeenable", "0", FCVAR_CHEAT, "Draw impact debris effects.");
static ConVar tf_impactwatertime("tf_impactwatertime", "1.0f", FCVAR_CHEAT, "Draw impact debris effects.");
#endif

//-----------------------------------------------------------------------------
// Purpose: Trace from the shooter to the point of impact (another player,
//          world, etc.), but this time take into account water/slime surfaces.
//   Input: trace - initial trace from player to point of impact
//          vecStart - starting point of the trace 
//-----------------------------------------------------------------------------
void CTFPlayer::ImpactWaterTrace(trace_t &trace, const Vector &vecStart)
{
#ifdef CLIENT_DLL
	if (tf_impactwatertimeenable.GetBool())
	{
		if (m_flWaterImpactTime > gpGlobals->curtime)
			return;
	}
#endif 

	trace_t traceWater;
	UTIL_TraceLine(vecStart, trace.endpos, (MASK_SHOT | CONTENTS_WATER | CONTENTS_SLIME),
		this, COLLISION_GROUP_NONE, &traceWater);
	if (traceWater.fraction < 1.0f)
	{
		CEffectData	data;
		data.m_vOrigin = traceWater.endpos;
		data.m_vNormal = traceWater.plane.normal;
		data.m_flScale = random->RandomFloat(8, 12);
		if (traceWater.contents & CONTENTS_SLIME)
		{
			data.m_fFlags |= FX_WATER_IN_SLIME;
		}

		const char *pszEffectName = "tf_gunshotsplash";
		CTFWeaponBase *pWeapon = GetActiveTFWeapon();
		if (pWeapon && (TF_WEAPON_MINIGUN == pWeapon->GetWeaponID()))
		{
			// for the minigun, use a different, cheaper splash effect because it can create so many of them
			pszEffectName = "tf_gunshotsplash_minigun";
		}
		DispatchEffect(pszEffectName, data);

#ifdef CLIENT_DLL
		if (tf_impactwatertimeenable.GetBool())
		{
			m_flWaterImpactTime = gpGlobals->curtime + tf_impactwatertime.GetFloat();
		}
#endif
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFWeaponBase *CTFPlayer::GetActiveTFWeapon( void ) const
{
	CBaseCombatWeapon *pRet = GetActiveWeapon();
	if ( pRet )
	{
		Assert( dynamic_cast< CTFWeaponBase* >( pRet ) != NULL );
		return static_cast< CTFWeaponBase * >( pRet );
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::IsActiveTFWeapon( int iWeaponID )
{
	CTFWeaponBase *pWeapon = GetActiveTFWeapon();
	if (pWeapon)
	{
		return pWeapon->GetWeaponID() == iWeaponID;
	}
	return false;
}

//-----------------------------------------------------------------------------
// Purpose: How much build resource ( metal ) does this player have
//-----------------------------------------------------------------------------
int CTFPlayer::GetBuildResources( void )
{
	return GetAmmoCount( TF_AMMO_METAL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::TeamFortress_SetSpeed()
{
	int playerclass = GetPlayerClass()->GetClassIndex();
	float maxfbspeed;

	// Spectators can move while in Classic Observer mode
	if ( IsObserver() )
	{
		if ( GetObserverMode() == OBS_MODE_ROAMING )
			SetMaxSpeed( GetPlayerClassData( TF_CLASS_SCOUT )->m_flMaxSpeed );
		else
			SetMaxSpeed( 0 );
		return;
	}

	// Check for any reason why they can't move at all
	if ( playerclass == TF_CLASS_UNDEFINED || GameRules()->InRoundRestart() )
	{
		SetAbsVelocity( vec3_origin );
		SetMaxSpeed( 1 );
		return;
	}

	// First, get their max class speed
	maxfbspeed = GetPlayerClassData( playerclass )->m_flMaxSpeed;

	CALL_ATTRIB_HOOK_FLOAT( maxfbspeed, mult_player_movespeed );

	// speed boost
	if ( m_Shared.IsSpeedBoosted() )
	{
		maxfbspeed *= 1.2f;
	}

	// Slow us down if we're disguised as a slower class
	// unless we're cloaked..
	if ( m_Shared.InCond( TF_COND_DISGUISED ) && !m_Shared.InCond( TF_COND_STEALTHED ) )
	{
		float flMaxDisguiseSpeed = GetPlayerClassData( m_Shared.GetDisguiseClass() )->m_flMaxSpeed;
		maxfbspeed = min( flMaxDisguiseSpeed, maxfbspeed );
	}

	// Second, see if any flags are slowing them down
	if ( HasItem() && GetItem()->GetItemID() == TF_ITEM_CAPTURE_FLAG )
	{
		CCaptureFlag *pFlag = dynamic_cast< CCaptureFlag *>( GetItem() );

		if ( pFlag )
		{
			if ( pFlag->GetGameType() == TF_FLAGTYPE_ATTACK_DEFEND || pFlag->GetGameType() == TF_FLAGTYPE_TERRITORY_CONTROL )
			{
				maxfbspeed *= 0.5;
			}
		}
	}

	// if they're aiming or spun up, reduce their speed
	if ( m_Shared.InCond( TF_COND_AIMING ) )
	{
		CTFWeaponBase *pWeapon = GetActiveTFWeapon();

		// Heavy moves slightly faster spun-up
		if ( pWeapon && pWeapon->IsWeapon( TF_WEAPON_MINIGUN ) )
		{
			if ( maxfbspeed > 110 )
				maxfbspeed = 110;
		}
		else
		{
			if ( maxfbspeed > 80 )
				maxfbspeed = 80;
		}
	}

	// Engineer moves slower while a hauling an object.
	if ( playerclass == TF_CLASS_ENGINEER && m_Shared.IsCarryingObject() )
	{
		maxfbspeed *= 0.75f;
	}

	if ( m_Shared.InCond( TF_COND_STEALTHED ) )
	{
		if ( maxfbspeed > tf_spy_max_cloaked_speed.GetFloat() )
			maxfbspeed = tf_spy_max_cloaked_speed.GetFloat();
	}

	if ( m_Shared.InCond( TF_COND_DISGUISED_AS_DISPENSER ) )
	{
		maxfbspeed *= 2.0f;
	}

	// (Potentially) Reduce our speed if we were stunned
	if ( m_Shared.InCond( TF_COND_STUNNED ) && ( m_Shared.m_nStunFlags & TF_STUNFLAG_SLOWDOWN ) )
	{
		maxfbspeed *= m_Shared.m_flStunMovementSpeed;
	}

	// if we're in bonus time because a team has won, give the winners 110% speed and the losers 90% speed
	if ( TFGameRules()->State_Get() == GR_STATE_TEAM_WIN )
	{
		int iWinner = TFGameRules()->GetWinningTeam();

		if ( iWinner != TEAM_UNASSIGNED )
		{
			if ( iWinner == GetTeamNumber() )
			{
				maxfbspeed *= 1.1f;
			}
			else
			{
				maxfbspeed *= 0.9f;
			}
		}
	}

	// Set the speed
	SetMaxSpeed( maxfbspeed );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFPlayer::HasItem(void)
{
	return (m_hItem != NULL);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CTFPlayer::SetItem(CTFItem *pItem)
{
	m_hItem = pItem;

#ifndef CLIENT_DLL
	if (pItem && pItem->GetItemID() == TF_ITEM_CAPTURE_FLAG)
	{
		RemoveInvisibility();
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTFItem	*CTFPlayer::GetItem(void)
{
	return m_hItem;
}

//-----------------------------------------------------------------------------
// Purpose: Is the player allowed to use a teleporter ?
//-----------------------------------------------------------------------------
bool CTFPlayer::HasTheFlag(void)
{
	if (HasItem() && GetItem()->GetItemID() == TF_ITEM_CAPTURE_FLAG)
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Are we allowed to pick the flag up?
//-----------------------------------------------------------------------------
bool CTFPlayer::IsAllowedToPickUpFlag(void)
{
	int bNotAllowedToPickUpFlag = 0;
	CALL_ATTRIB_HOOK_INT(bNotAllowedToPickUpFlag, cannot_pick_up_intelligence);

	if (bNotAllowedToPickUpFlag > 0)
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this player's allowed to build another one of the specified object
//-----------------------------------------------------------------------------
int CTFPlayer::CanBuild(int iObjectType, int iObjectMode)
{
	if (iObjectType < 0 || iObjectType >= OBJ_LAST)
		return CB_UNKNOWN_OBJECT;

#ifndef CLIENT_DLL
	CTFPlayerClass *pCls = GetPlayerClass();

	if (m_Shared.IsCarryingObject())
	{
		CBaseObject *pObject = m_Shared.GetCarriedObject();
		if (pObject && pObject->GetType() == iObjectType && pObject->GetObjectMode() == iObjectMode)
		{
			return CB_CAN_BUILD;
		}
		else
		{
			Assert(0);
		}
	}

	if (pCls && pCls->CanBuildObject(iObjectType) == false)
	{
		return CB_CANNOT_BUILD;
	}

	if (GetObjectInfo(iObjectType)->m_AltModes.Count() != 0
		&& GetObjectInfo(iObjectType)->m_AltModes.Count() <= iObjectMode * 3)
	{
		return CB_CANNOT_BUILD;
	}
	else if (GetObjectInfo(iObjectType)->m_AltModes.Count() == 0 && iObjectMode != 0)
	{
		return CB_CANNOT_BUILD;
	}

#endif

	int iObjectCount = GetNumObjects(iObjectType, iObjectMode);

	// Make sure we haven't hit maximum number
	if (iObjectCount >= GetObjectInfo(iObjectType)->m_nMaxObjects && GetObjectInfo(iObjectType)->m_nMaxObjects != -1)
	{
		return CB_LIMIT_REACHED;
	}

	// Find out how much the object should cost
	int iCost = CalculateObjectCost( iObjectType, HasGunslinger() );

	// Make sure we have enough resources
	if (GetBuildResources() < iCost)
	{
		return CB_NEED_RESOURCES;
	}

	return CB_CAN_BUILD;
}

//-----------------------------------------------------------------------------
// Purpose: Get the number of objects of the specified type that this player has
//-----------------------------------------------------------------------------
int CTFPlayer::GetNumObjects(int iObjectType, int iObjectMode)
{
	int iCount = 0;
	for (int i = 0; i < GetObjectCount(); i++)
	{
		if (!GetObject(i))
			continue;

		if (GetObject(i)->GetType() == iObjectType && GetObject(i)->GetObjectMode() == iObjectMode && !GetObject(i)->IsBeingCarried())
		{
			iCount++;
		}
	}

	return iCount;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::ItemPostFrame()
{
	if (m_hOffHandWeapon.Get() && m_hOffHandWeapon->IsWeaponVisible())
	{
		if (gpGlobals->curtime < m_flNextAttack)
		{
			m_hOffHandWeapon->ItemBusyFrame();
		}
		else
		{
#if defined( CLIENT_DLL )
			// Not predicting this weapon
			if (m_hOffHandWeapon->IsPredicted())
#endif
			{
				m_hOffHandWeapon->ItemPostFrame();
			}
		}
	}

	BaseClass::ItemPostFrame();
}

void CTFPlayer::SetOffHandWeapon(CTFWeaponBase *pWeapon)
{
	m_hOffHandWeapon = pWeapon;
	if (m_hOffHandWeapon.Get())
	{
		m_hOffHandWeapon->Deploy();
	}
}

// Set to NULL at the end of the holster?
void CTFPlayer::HolsterOffHandWeapon(void)
{
	if (m_hOffHandWeapon.Get())
	{
		m_hOffHandWeapon->Holster();
	}
}

//-----------------------------------------------------------------------------
// Purpose: Return true if we should record our last weapon when switching between the two specified weapons
//-----------------------------------------------------------------------------
bool CTFPlayer::Weapon_ShouldSetLast(CBaseCombatWeapon *pOldWeapon, CBaseCombatWeapon *pNewWeapon)
{
	// if the weapon doesn't want to be auto-switched to, don't!	
	CTFWeaponBase *pTFWeapon = dynamic_cast< CTFWeaponBase * >(pOldWeapon);

	if (pTFWeapon->AllowsAutoSwitchTo() == false)
	{
		return false;
	}

	return BaseClass::Weapon_ShouldSetLast(pOldWeapon, pNewWeapon);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CTFPlayer::Weapon_Switch(CBaseCombatWeapon *pWeapon, int viewmodelindex)
{
	m_PlayerAnimState->ResetGestureSlot(GESTURE_SLOT_ATTACK_AND_RELOAD);
	return BaseClass::Weapon_Switch(pWeapon, viewmodelindex);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::GetStepSoundVelocities(float *velwalk, float *velrun)
{
	float flMaxSpeed = MaxSpeed();

	if ((GetFlags() & FL_DUCKING) || (GetMoveType() == MOVETYPE_LADDER))
	{
		*velwalk = flMaxSpeed * 0.25;
		*velrun = flMaxSpeed * 0.3;
	}
	else
	{
		*velwalk = flMaxSpeed * 0.3;
		*velrun = flMaxSpeed * 0.8;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFPlayer::SetStepSoundTime(stepsoundtimes_t iStepSoundTime, bool bWalking)
{
	float flMaxSpeed = MaxSpeed();

	switch (iStepSoundTime)
	{
	case STEPSOUNDTIME_NORMAL:
	case STEPSOUNDTIME_WATER_FOOT:
		m_flStepSoundTime = RemapValClamped(flMaxSpeed, 200, 450, 400, 200);
		if (bWalking)
		{
			m_flStepSoundTime += 100;
		}
		break;

	case STEPSOUNDTIME_ON_LADDER:
		m_flStepSoundTime = 350;
		break;

	case STEPSOUNDTIME_WATER_KNEE:
		m_flStepSoundTime = RemapValClamped(flMaxSpeed, 200, 450, 600, 400);
		break;

	default:
		Assert(0);
		break;
	}

	if ((GetFlags() & FL_DUCKING) || (GetMoveType() == MOVETYPE_LADDER))
	{
		m_flStepSoundTime += 100;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::CanAttack(void)
{
	CTFGameRules *pRules = TFGameRules();

	Assert(pRules);

	if (m_Shared.GetStealthNoAttackExpireTime() > gpGlobals->curtime || m_Shared.InCond(TF_COND_STEALTHED))
	{
#ifdef CLIENT_DLL
		HintMessage(HINT_CANNOT_ATTACK_WHILE_CLOAKED, true, true);
#endif
		return false;
	}

	// Stunned players cannot attack
	if ( m_Shared.InCond( TF_COND_STUNNED ) )
		return false;

	if ((pRules->State_Get() == GR_STATE_TEAM_WIN) && (pRules->GetWinningTeam() != GetTeamNumber()))
	{
		return false;
	}

	return true;
}

#ifdef GAME_DLL
bool CTFPlayer::CanPickupBuilding(CBaseObject *pObject)
{
	if ( !pObject )
		return false;

	if ( pObject->GetBuilder() != this )
		return false;

	if ( pObject->IsBuilding() || pObject->IsUpgrading() || pObject->IsRedeploying() )
		return false;

	if ( pObject->HasSapper() )
		return false;

	return true;

	if ( m_Shared.InCond( TF_COND_STUNNED ) )
	{
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::TryToPickupBuilding( void )
{
	if ( !tf2c_building_hauling.GetBool() )
		return false;

	if ( m_Shared.IsLoser() )
		return false;

	if ( IsActiveTFWeapon( TF_WEAPON_BUILDER ) )
		return false;

	int bCannotPickUpBuildings = 0;
	CALL_ATTRIB_HOOK_INT( bCannotPickUpBuildings, cannot_pick_up_buildings );
	if ( bCannotPickUpBuildings != 0 )
		return false;

	Vector vecForward;
	AngleVectors( EyeAngles(), &vecForward );
	Vector vecSwingStart = Weapon_ShootPosition();
	// 5500 with Rescue Ranger.
	float flRange = 150.0f;
	Vector vecSwingEnd = vecSwingStart + vecForward * flRange;

	// only trace against objects

	// See if we hit anything.
	trace_t trace;

	CTraceFilterIgnorePlayers traceFilter( NULL, COLLISION_GROUP_NONE );
	UTIL_TraceLine( vecSwingStart, vecSwingEnd, MASK_SOLID, &traceFilter, &trace );

	if ( trace.fraction < 1.0f &&
		trace.m_pEnt &&
		trace.m_pEnt->IsBaseObject() &&
		trace.m_pEnt->GetTeamNumber() == GetTeamNumber() )
	{
		CBaseObject *pObject = dynamic_cast<CBaseObject *>( trace.m_pEnt );
		if ( CanPickupBuilding( pObject ) )
		{
			CTFWeaponBase *pWpn = Weapon_OwnsThisID( TF_WEAPON_BUILDER );

			if ( pWpn )
			{
				CTFWeaponBuilder *pBuilder = dynamic_cast< CTFWeaponBuilder * >( pWpn );

				// Is this the builder that builds the object we're looking for?
				if ( pBuilder )
				{
					pObject->MakeCarriedObject(this);

					pBuilder->SetSubType( pObject->ObjectType() );
					pBuilder->SetObjectMode( pObject->GetObjectMode() );

					SpeakConceptIfAllowed( MP_CONCEPT_PICKUP_BUILDING );

					// try to switch to this weapon
					Weapon_Switch( pBuilder );

					m_flNextCarryTalkTime = gpGlobals->curtime + RandomFloat( 6.0f, 12.0f );

					return true;
				}
			}
		}
	}
	return false;
}

void CTFPlayerShared::SetCarriedObject(CBaseObject *pObj)
{
	if (pObj)
	{
		m_bCarryingObject = true;
		m_hCarriedObject = pObj;
	}
	else
	{
		m_bCarryingObject = false;
		m_hCarriedObject = NULL;
	}

	m_pOuter->TeamFortress_SetSpeed();
}

CBaseObject* CTFPlayerShared::GetCarriedObject(void)
{
	CBaseObject *pObj = m_hCarriedObject.Get();
	return pObj;
}

#endif

//-----------------------------------------------------------------------------
// Purpose: Weapons can call this on secondary attack and it will link to the class
// ability
//-----------------------------------------------------------------------------
bool CTFPlayer::DoClassSpecialSkill(void)
{
	bool bDoSkill = false;

	switch (GetPlayerClass()->GetClassIndex())
	{
	case TF_CLASS_SPY:
	{
		if (m_Shared.m_flStealthNextChangeTime <= gpGlobals->curtime)
		{
			// Toggle invisibility
			if (m_Shared.InCond(TF_COND_STEALTHED))
			{
#ifdef GAME_DLL
				m_Shared.FadeInvis(tf_spy_invis_unstealth_time.GetFloat());
#endif
				bDoSkill = true;
			}
			else if (CanGoInvisible() && (m_Shared.GetSpyCloakMeter() > 8.0f))	// must have over 10% cloak to start
			{
#ifdef GAME_DLL
				m_Shared.AddCond(TF_COND_STEALTHED);
#endif
				bDoSkill = true;
			}

			if (bDoSkill)
				m_Shared.m_flStealthNextChangeTime = gpGlobals->curtime + 0.5;
		}
	}
	break;

	case TF_CLASS_DEMOMAN:
	{
		CTFPipebombLauncher *pPipebombLauncher = static_cast<CTFPipebombLauncher*>(Weapon_OwnsThisID(TF_WEAPON_PIPEBOMBLAUNCHER));

		if (pPipebombLauncher)
		{
			pPipebombLauncher->SecondaryAttack();
		}
	}
	bDoSkill = true;
	break;

	case TF_CLASS_ENGINEER:
	{
		bDoSkill = false;
#ifdef GAME_DLL
		bDoSkill = TryToPickupBuilding();
#endif
	}
	break;

	default:
		break;
	}

	return bDoSkill;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CTFPlayer::CanGoInvisible(void)
{
	if (HasItem() && GetItem()->GetItemID() == TF_ITEM_CAPTURE_FLAG)
	{
		HintMessage(HINT_CANNOT_CLOAK_WITH_FLAG);
		return false;
	}

	// Stunned players cannot go invisible
	if ( m_Shared.InCond( TF_COND_STUNNED ) )
	{
		return false;
	}

	CTFGameRules *pRules = TFGameRules();

	Assert(pRules);

	if ((pRules->State_Get() == GR_STATE_TEAM_WIN) && (pRules->GetWinningTeam() != GetTeamNumber()))
	{
		return false;
	}

	return true;
}

//ConVar testclassviewheight( "testclassviewheight", "0", FCVAR_DEVELOPMENTONLY | FCVAR_REPLICATED );
//Vector vecTestViewHeight(0,0,0);

//-----------------------------------------------------------------------------
// Purpose: Return class-specific standing eye height
//-----------------------------------------------------------------------------
Vector CTFPlayer::GetClassEyeHeight(void)
{
	CTFPlayerClass *pClass = GetPlayerClass();

	if (!pClass)
		return VEC_VIEW_SCALED(this);

	/*if ( testclassviewheight.GetFloat() > 0 )
	{
	vecTestViewHeight.z = testclassviewheight.GetFloat();
	return vecTestViewHeight;
	}*/

	int iClassIndex = pClass->GetClassIndex();

	if (iClassIndex < TF_FIRST_NORMAL_CLASS || iClassIndex > TF_CLASS_COUNT)
		return VEC_VIEW_SCALED(this);

	return (g_TFClassViewVectors[pClass->GetClassIndex()] * GetModelScale());
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CTFPlayer::MedicGetChargeLevel( void )
{
	CWeaponMedigun *pMedigun = GetMedigun();

	if ( pMedigun )
		return pMedigun->GetChargeLevel();

	return 0.0f;

	// Spy has a fake uber level.
	if ( IsPlayerClass( TF_CLASS_SPY ) )
	{
		return m_Shared.m_flDisguiseChargeLevel;
	}

	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponMedigun *CTFPlayer::GetMedigun(void)
{
	CTFWeaponBase *pWeapon = Weapon_OwnsThisID( TF_WEAPON_MEDIGUN );
	if ( pWeapon )
		return static_cast<CWeaponMedigun *>( pWeapon );

	return NULL;
}

CTFWeaponBase *CTFPlayer::Weapon_OwnsThisID( int iWeaponID )
{
	for (int i = 0; i < WeaponCount(); i++)
	{
		CTFWeaponBase *pWpn = ( CTFWeaponBase *)GetWeapon( i );

		if ( pWpn == NULL )
			continue;

		if ( pWpn->GetWeaponID() == iWeaponID )
		{
			return pWpn;
		}
	}

	return NULL;
}

CTFWeaponBase *CTFPlayer::Weapon_GetWeaponByType(int iType)
{
	for ( int i = 0; i < WeaponCount(); i++ )
	{
		CTFWeaponBase *pWpn = ( CTFWeaponBase * )GetWeapon( i );

		if ( pWpn == NULL )
			continue;

		int iWeaponRole = pWpn->GetTFWpnData().m_iWeaponType;

		if ( iWeaponRole == iType )
		{
			return pWpn;
		}
	}

	return NULL;

}

CEconEntity *CTFPlayer::GetEntityForLoadoutSlot(int iSlot)
{
	if ( iSlot >= TF_LOADOUT_SLOT_HAT )
	{
		// Weapons don't get equipped in cosmetic slots.
		return GetWearableForLoadoutSlot( iSlot );
	}

	int iClass = m_PlayerClass.GetClassIndex();

	for ( int i = 0; i < WeaponCount(); i++ )
	{
		CBaseCombatWeapon *pWeapon = GetWeapon( i );
		if ( !pWeapon )
			continue;

		CEconItemDefinition *pItemDef = pWeapon->GetItem()->GetStaticData();

		if ( pItemDef && pItemDef->GetLoadoutSlot(iClass) == iSlot )
		{
			return pWeapon;
		}
	}

	// Wearable?
	CEconWearable *pWearable = GetWearableForLoadoutSlot(iSlot);
	if ( pWearable )
		return pWearable;

	return NULL;
}

CEconWearable *CTFPlayer::GetWearableForLoadoutSlot(int iSlot)
{
	int iClass = m_PlayerClass.GetClassIndex();

	for (int i = 0; i < GetNumWearables(); i++)
	{
		CEconWearable *pWearable = GetWearable(i);

		if (!pWearable)
			continue;

		CEconItemDefinition *pItemDef = pWearable->GetItem()->GetStaticData();

		if (pItemDef && pItemDef->GetLoadoutSlot(iClass) == iSlot)
		{
			return pWearable;
		}
	}

	return NULL;
}

int CTFPlayer::GetMaxAmmo( int iAmmoIndex, int iClassNumber /*= -1*/ )
{
	if ( !GetPlayerClass()->GetData() )
		return 0;

	int iMaxAmmo = 0;

	if ( iClassNumber != -1 )
	{
		iMaxAmmo = GetPlayerClassData( iClassNumber )->m_aAmmoMax[iAmmoIndex];
	}
	else
	{
		iMaxAmmo = GetPlayerClass()->GetData()->m_aAmmoMax[iAmmoIndex];
	}

	// If we have a weapon that overrides max ammo, use its value.
	// BUG: If player has multiple weapons using same ammo type then only the first one's value is used.
	for ( int i = 0; i < WeaponCount(); i++ )
	{
		CTFWeaponBase *pWpn = (CTFWeaponBase *)GetWeapon( i );

		if ( !pWpn )
			continue;

		if ( pWpn->GetPrimaryAmmoType() != iAmmoIndex )
			continue;

		// Random weapons should use the ammo of the player class related to this weapon
		// BUG: Client calls to this method will return incorrect max ammo values in randomizer
#ifdef GAME_DLL
		if ( tf2c_random_weapons.GetBool() )
		{
			CEconItemView *pItem = pWpn->GetItem();
			if ( pItem )
			{
				iMaxAmmo = GetPlayerClassData( pItem->GetItemClassNumber() )->m_aAmmoMax[iAmmoIndex];
			}
		}
#endif
	}

	switch ( iAmmoIndex )
	{
	case TF_AMMO_PRIMARY:
		CALL_ATTRIB_HOOK_INT( iMaxAmmo, mult_maxammo_primary );
		break;

	case TF_AMMO_SECONDARY:
		CALL_ATTRIB_HOOK_INT( iMaxAmmo, mult_maxammo_secondary );
		break;

	case TF_AMMO_METAL:
		CALL_ATTRIB_HOOK_INT( iMaxAmmo, mult_maxammo_metal );
		break;

	case TF_AMMO_GRENADES1:
		CALL_ATTRIB_HOOK_INT( iMaxAmmo, mult_maxammo_grenades1 );
		break;

	case 6:
	default:
		iMaxAmmo = 1;
		break;
	}

	return iMaxAmmo;
}

void CTFPlayer::PlayStepSound(Vector &vecOrigin, surfacedata_t *psurface, float fvol, bool force)
{
#ifdef CLIENT_DLL
	// Don't make predicted footstep sounds in third person, animevents will take care of that.
	if (prediction->InPrediction() && C_BasePlayer::ShouldDrawLocalPlayer())
		return;
#endif

	BaseClass::PlayStepSound(vecOrigin, psurface, fvol, force);
}