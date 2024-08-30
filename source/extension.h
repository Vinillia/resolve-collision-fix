/**
 * vim: set ts=4 :
 * =============================================================================
 * SourceMod Sample Extension
 * Copyright (C) 2004-2008 AlliedModders LLC.  All rights reserved.
 * =============================================================================
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License, version 3.0, as published by the
 * Free Software Foundation.
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, AlliedModders LLC gives you permission to link the
 * code of this program (as well as its derivative works) to "Half-Life 2," the
 * "Source Engine," the "SourcePawn JIT," and any Game MODs that run on software
 * by the Valve Corporation.  You must obey the GNU General Public License in
 * all respects for all other code used.  Additionally, AlliedModders LLC grants
 * this exception to all derivative works.  AlliedModders LLC defines further
 * exceptions, found in LICENSE.txt (as of this writing, version JULY-31-2007),
 * or <http://www.sourcemod.net/license.php>.
 *
 * Version: $Id$
 */

#ifndef _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_
#define _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_

/**
 * @file extension.h
 * @brief Sample extension code header.
 */

#include <vphysics_interface.h>
#include <IEngineTrace.h>
#include <IStaticPropMgr.h>
#include "debugoverlay.h"

#undef clamp

#include "smsdk_ext.h"
#include <ISDKHooks.h>

/**
 * @brief Sample implementation of the SDK Extension.
 * Note: Uncomment one of the pre-defined virtual functions in order to use it.
 */
class SDKResolveCollision : public SDKExtension, public IConCommandBaseAccessor, public ISMEntityListener
{
public: // SDKExtension
	virtual bool SDK_OnLoad(char* error, size_t maxlen, bool late) override;
	virtual void SDK_OnUnload() override;
	virtual void SDK_OnAllLoaded() override;
	// virtual void SDK_OnPauseChange(bool paused) override;
	virtual bool QueryRunning(char* error, size_t maxlen) override;

public: // SDKExtension MetaMod
#if defined SMEXT_CONF_METAMOD
	virtual bool SDK_OnMetamodLoad(ISmmAPI* ismm, char* error, size_t maxlen, bool late);
	virtual bool SDK_OnMetamodUnload(char* error, size_t maxlen) override;
	// virtual bool SDK_OnMetamodPauseChange(bool paused, char* error, size_t maxlen) override;
#endif

public: // IConCommandBaseAccessor
	virtual bool RegisterConCommandBase(ConCommandBase* command) override;
};

extern SDKResolveCollision g_sdkResolveCollision;
extern ICvar* icvar;
extern CGlobalVars* gpGlobals;
extern IEngineTrace* enginetrace;
extern IStaticPropMgrServer* staticpropmgr;
extern IPhysics* iphysics;
extern CDebugOverlay* debugoverlay;

extern ConVar z_resolve_collision;
extern ConVar z_resolve_collision_debug;

extern ConVar z_resolve_zombie_collision;
extern ConVar z_resolve_zombie_collision_multiplier;

extern ConVar z_resolve_zombie_climb_up_ledge;
extern ConVar z_resolve_zombie_climb_up_ledge_debug;
extern ConVar z_resolve_zombie_climb_up_slope_timer;

#endif // _INCLUDE_SOURCEMOD_EXTENSION_PROPER_H_
