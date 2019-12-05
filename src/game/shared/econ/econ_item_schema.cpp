#include "cbase.h"
#include "econ_item_schema.h"
#include "econ_item_system.h"
#include "tier3/tier3.h"
#include "vgui/ILocalize.h"

//-----------------------------------------------------------------------------
// CEconItemAttribute
//-----------------------------------------------------------------------------

BEGIN_NETWORK_TABLE_NOBASE( CEconItemAttribute, DT_EconItemAttribute )
#ifdef CLIENT_DLL
	RecvPropInt( RECVINFO( m_iAttributeDefinitionIndex ) ),
	RecvPropInt( RECVINFO( m_iRawValue32 ) ),
#else
	SendPropInt( SENDINFO( m_iAttributeDefinitionIndex ) ),
	SendPropInt( SENDINFO( m_iRawValue32 ), -1, SPROP_UNSIGNED ),
#endif
END_NETWORK_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemAttribute::Init( int iIndex, float flValue, const char *pszAttributeClass /*= NULL*/ )
{
	m_iAttributeDefinitionIndex = iIndex;
	
	attrib_data_union_t value;
	value.flVal = flValue;
	m_iRawValue32 = value.iVal;

	if ( pszAttributeClass )
	{
		m_iAttributeClass = AllocPooledString( pszAttributeClass );
	}
	else
	{
		EconAttributeDefinition *pAttribDef = GetStaticData();
		if ( pAttribDef )
		{
			m_iAttributeClass = AllocPooledString( pAttribDef->attribute_class );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CEconItemAttribute::Init( int iIndex, const char *pszValue, const char *pszAttributeClass /*= NULL*/ )
{
	m_iAttributeDefinitionIndex = iIndex;

	attrib_data_union_t value;
	value.sVal = AllocPooledString_StaticConstantStringPointer( pszValue );
	m_iRawValue32 = value.iVal;

	if ( pszAttributeClass )
	{
		m_iAttributeClass = AllocPooledString( pszAttributeClass );
	}
	else
	{
		EconAttributeDefinition *pAttribDef = GetStaticData();
		if ( pAttribDef )
		{
			m_iAttributeClass = AllocPooledString( pAttribDef->attribute_class );
		}
	}
}

EconAttributeDefinition *CEconItemAttribute::GetStaticData( void )
{
	return GetItemSchema()->GetAttributeDefinition( m_iAttributeDefinitionIndex );
}



//-----------------------------------------------------------------------------
// CEconItemDefinition
//-----------------------------------------------------------------------------

PerTeamVisuals_t *CEconItemDefinition::GetVisuals( int iTeamNum /*= TEAM_UNASSIGNED*/ )
{
	if ( iTeamNum > LAST_SHARED_TEAM && iTeamNum < TF_TEAM_COUNT )
	{
		return &visual[iTeamNum];
	}

	return &visual[TEAM_UNASSIGNED];
}

int CEconItemDefinition::GetLoadoutSlot( int iClass /*= TF_CLASS_UNDEFINED*/ )
{
	if ( iClass && item_slot_per_class[iClass] != -1 )
	{
		return item_slot_per_class[iClass];
	}

	return item_slot;
}

//-----------------------------------------------------------------------------
// Purpose: Generate item name to show in UI with prefixes, qualities, etc...
//-----------------------------------------------------------------------------
const wchar_t *CEconItemDefinition::GenerateLocalizedFullItemName( void )
{
	static wchar_t wszFullName[256];
	wszFullName[0] = '\0';

	wchar_t wszPrefix[128];
	wchar_t wszQuality[128];
	wszPrefix[0] = '\0';
	wszQuality[0] = '\0';


	if ( propername )
	{
		const wchar_t *pszPrepend = g_pVGuiLocalize->Find( "#TF_Unique_Prepend_Proper_Quality" );

		if ( pszPrepend )
		{
			V_wcsncpy( wszPrefix, pszPrepend, sizeof( wszPrefix ) );
		}
	}
	
	// Quality prefixes are a bit annoying, so don't bother loading them.
	/* 
	if ( item_quality != QUALITY_NORMAL )
	{
		// Live TF2 allows multiple qualities per item but eh, we don't need that for now.
		const wchar_t *pszQuality = g_pVGuiLocalize->Find( g_szQualityLocalizationStrings[item_quality] );

		if ( pszQuality )
		{
			V_wcsncpy( wszQuality, pszQuality, sizeof( wszQuality ) );
		}
	} */

	// Attach the original item name after we're done with all the prefixes.
	wchar_t wszItemName[256];

	const wchar_t *pszLocalizedName = g_pVGuiLocalize->Find( item_name );
	if ( pszLocalizedName && pszLocalizedName[0] )
	{
		V_wcsncpy( wszItemName, pszLocalizedName, sizeof( wszItemName ) );
	}
	else
	{
		g_pVGuiLocalize->ConvertANSIToUnicode( item_name, wszItemName, sizeof( wszItemName ) );
	}

	if ( ( wszQuality[0] == '\0') && ( wszPrefix[0] != '\0') ) // Propername, but no Quality
	{
		g_pVGuiLocalize->ConstructString( wszFullName, sizeof( wszFullName ), L"%s1 %s2", 2,
		wszPrefix, wszItemName );
	}
	else if ( ( wszQuality[0] != '\0') && ( wszPrefix[0] != '\0') ) // Quality and Propername
	{
		g_pVGuiLocalize->ConstructString( wszFullName, sizeof( wszFullName ), L"%s1 %s2 %s3", 3,
		wszPrefix, wszQuality, wszItemName );
	}
	else if ( ( wszQuality[0] != '\0') && ( wszPrefix[0] == '\0') ) // Quality, but no Propername
	{
		g_pVGuiLocalize->ConstructString( wszFullName, sizeof( wszFullName ), L"%s1 %s2", 2,
		wszQuality, wszItemName );
	}
	else // No Quality or Propername
	{
		g_pVGuiLocalize->ConstructString( wszFullName, sizeof( wszFullName ), L"%s1", 1,
		wszItemName );
	}

	return wszFullName;
}

//-----------------------------------------------------------------------------
// Purpose: Generate item name without qualities, prefixes, etc. for disguise HUD...
//-----------------------------------------------------------------------------
const wchar_t *CEconItemDefinition::GenerateLocalizedItemNameNoQuality( void )
{
	static wchar_t wszFullName[256];
	wszFullName[0] = '\0';

	wchar_t wszPrefix[128];
	wszPrefix[0] = '\0';

	// Attach "the" if necessary.
	if ( propername )
	{
		const wchar_t *pszPrepend = g_pVGuiLocalize->Find( "#TF_Unique_Prepend_Proper_Quality" );

		if ( pszPrepend )
		{
			V_wcsncpy( wszPrefix, pszPrepend, sizeof( wszPrefix ) );
		}
	}

	// Attach the original item name after we're done with all the prefixes.
	wchar_t wszItemName[256];

	const wchar_t *pszLocalizedName = g_pVGuiLocalize->Find( item_name );
	if ( pszLocalizedName && pszLocalizedName[0] )
	{
		V_wcsncpy( wszItemName, pszLocalizedName, sizeof( wszItemName ) );
	}
	else
	{
		g_pVGuiLocalize->ConvertANSIToUnicode( item_name, wszItemName, sizeof( wszItemName ) );
	}

	g_pVGuiLocalize->ConstructString( wszFullName, sizeof( wszFullName ), L"%s1 %s2", 2,
		wszPrefix, wszItemName );

	return wszFullName;
}


void CEconItemDefinition::IterateAttributes( IEconAttributeIterator &iter )
{
	// Returning the first attribute found.
	FOR_EACH_VEC( attributes, i )
	{
		if ( !iter.OnIterateAttributeValue( attributes[i].GetStaticData(), attributes[i].m_iRawValue32 ) )
			return;
	}
}
