#include "extension.h"

#include <CDetour/detours.h>
#include <compat_wrappers.h>
#include "resolve_collision.h"
#include "resolve_collision_tools.h"

SDKResolveCollision g_sdkResolveCollision;
SMEXT_LINK(&g_sdkResolveCollision);

IPhysics* iphysics = nullptr;
IGameConfig* gpConfig = nullptr;
CGlobalVars* gpGlobals = nullptr; 
ICvar* icvar = nullptr;
IEngineTrace* enginetrace = nullptr;
IStaticPropMgrServer* staticpropmgr = nullptr;
CDebugOverlay* debugoverlay = nullptr;
ISDKHooks* g_pSDKHooks = nullptr;

CDetour* g_pResolveCollisionDetour = nullptr;
CDetour* g_pResolveZombieCollisionDetour = nullptr;
CDetour* g_pResolveZombieClimbUpLedgeDetour = nullptr;
CDetour* g_pUpdateGroundConstraint = nullptr;
CDetour* g_pUpdatePosition = nullptr;

ConVar z_resolve_collision("z_resolve_collision", "1", 0, "0 - Use original function; 1 - Use extension implementation with fix; 2 - Use extension implementation without fix; 3 - Neither of calls");
ConVar z_resolve_collision_debug("z_resolve_collision_debug", "0", 0, "0 - Disable collision overlay;1 - Enable collision overlay; 2 - Enable clean collision overlay (works only for 1 common but smoother)");

ConVar z_resolve_zombie_collision("z_resolve_zombie_collision", "1", 0, "0 - Use original function; 1 - Use extension implementation");
ConVar z_resolve_zombie_collision_multiplier("z_resolve_zombie_collision_multiplier", "1.0", 0, "Multiplier of commons collision force");

ConVar z_resolve_zombie_climb_up_ledge("z_resolve_zombie_climb_up_ledge", "1", 0, "0 - Use original function; 1 - Use extension implementation");
ConVar z_resolve_zombie_climb_up_ledge_debug("z_resolve_zombie_climb_up_ledge_debug", "0", 0, "0 - Disable debug; 1 - Enable debug");

ConVar z_resolve_zombie_collision_auto_multiplier("z_resolve_zombie_collision_auto_multiplier", "1", 0, "Automaticly manages power of collision between common infected");

DETOUR_DECL_MEMBER1(NextBotGroundLocomotion__ResolveZombieCollisions, Vector, const Vector&, pos)
{
	NextBotGroundLocomotion* groundLocomotion = (NextBotGroundLocomotion*)this;

	if (z_resolve_zombie_collision.GetInt() == 1)
		return groundLocomotion->ResolveZombieCollisions(pos);

	return DETOUR_MEMBER_CALL(NextBotGroundLocomotion__ResolveZombieCollisions)(pos);
}

DETOUR_DECL_MEMBER3(NextBotGroundLocomotion__ResolveCollision, Vector, const Vector&, from, const Vector&, to, int, recursionLimit)
{
	NextBotGroundLocomotion* groundLocomotion = (NextBotGroundLocomotion*)this;
	
	if (z_resolve_collision.GetInt() == 3)
		return to;

	if (z_resolve_collision.GetBool())
		return groundLocomotion->ResolveCollision(from, to, recursionLimit);

	return DETOUR_MEMBER_CALL(NextBotGroundLocomotion__ResolveCollision)(from, to, recursionLimit);
}

DETOUR_DECL_MEMBER3(NextBotGroundLocomotion__ClimbUpToLedge, bool, const Vector&, landingGoal, const Vector&, landingForward, const CBaseEntity*, obstacle)
{
	NextBotGroundLocomotion* groundLocomotion = (NextBotGroundLocomotion*)this;

	if (z_resolve_zombie_climb_up_ledge.GetBool())
		return groundLocomotion->ClimbUpToLedgeThunk(landingGoal, landingForward, obstacle);

	return DETOUR_MEMBER_CALL(NextBotGroundLocomotion__ClimbUpToLedge)(landingGoal, landingForward, obstacle);
}

DETOUR_DECL_MEMBER0(ZombieBotLocomotion__UpdateGroundConstraint, void)
{
	NextBotGroundLocomotion* groundLocomotion = (NextBotGroundLocomotion*)this;

	if (z_resolve_zombie_climb_up_ledge.GetBool())
		return groundLocomotion->UpdateGroundConstraint();

	DETOUR_MEMBER_CALL(ZombieBotLocomotion__UpdateGroundConstraint)();
}

DETOUR_DECL_MEMBER1(ZombieBotLocomotion__UpdatePosition, void, const Vector&, newPos)
{
	NextBotGroundLocomotion* groundLocomotion = (NextBotGroundLocomotion*)this;

	if (z_resolve_collision.GetBool())
		return groundLocomotion->UpdatePosition(newPos);

	DETOUR_MEMBER_CALL(ZombieBotLocomotion__UpdatePosition)(newPos);
}

bool SDKResolveCollision::SDK_OnLoad(char* error, size_t maxlen, bool late)
{
	if (!gameconfs->LoadGameConfigFile("l4d2_resolve_collision", &gpConfig, error, maxlen))
		return false;

	if (!collisiontools->Initialize(gpConfig))
	{
		V_snprintf(error, maxlen, "Failed to initialize ResolveCollision tools");
		return false;
	}

	CDetourManager::Init(g_pSM->GetScriptingEngine(), gpConfig);

	g_pResolveCollisionDetour = DETOUR_CREATE_MEMBER(NextBotGroundLocomotion__ResolveCollision, "NextBotGroundLocomotion::ResolveCollision");
	g_pResolveZombieCollisionDetour = DETOUR_CREATE_MEMBER(NextBotGroundLocomotion__ResolveZombieCollisions, "NextBotGroundLocomotion::ResolveZombieCollisions");
	g_pResolveZombieClimbUpLedgeDetour = DETOUR_CREATE_MEMBER(NextBotGroundLocomotion__ClimbUpToLedge, "NextBotGroundLocomotion::ClimbUpToLedge");
	g_pUpdateGroundConstraint = DETOUR_CREATE_MEMBER(ZombieBotLocomotion__UpdateGroundConstraint, "ZombieBotLocomotion::UpdateGroundConstraint");
	
#ifdef _WIN32
	g_pUpdatePosition = DETOUR_CREATE_MEMBER(ZombieBotLocomotion__UpdatePosition, "ZombieBotLocomotion::UpdatePosition");
#endif // _WIN32

	if (g_pResolveCollisionDetour == nullptr)
	{
		V_snprintf(error, maxlen, "Failed to create NextBotGroundLocomotion::ResolveCollision detour");
		return false;
	}

	if (g_pResolveZombieCollisionDetour == nullptr)
	{
		V_snprintf(error, maxlen, "Failed to create NextBotGroundLocomotion::ResolveZombieCollisions detour");
		return false;
	}

	if (g_pResolveZombieClimbUpLedgeDetour == nullptr)
	{
		V_snprintf(error, maxlen, "Failed to create NextBotGroundLocomotion::ClimbUpToLedge detour");
		return false;
	}

	if (g_pUpdateGroundConstraint == nullptr)
	{
		V_snprintf(error, maxlen, "Failed to create ZombieBotLocomotion::UpdateGroundConstraint detour");
		return false;
	}

#ifdef _WIN32
	if (g_pUpdatePosition == nullptr)
	{
		V_snprintf(error, maxlen, "Failed to create ZombieBotLocomotion::UpdatePosition detour");
		return false;
	}
#endif

	g_pUpdateGroundConstraint->EnableDetour();
	g_pResolveCollisionDetour->EnableDetour();
	g_pResolveZombieCollisionDetour->EnableDetour();
	g_pResolveZombieClimbUpLedgeDetour->EnableDetour();

#ifdef _WIN32
	g_pUpdatePosition->EnableDetour();
#endif

	return true;
}

void SDKResolveCollision::SDK_OnAllLoaded()
{
}

bool SDKResolveCollision::SDK_OnMetamodLoad(ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
#ifdef WIN32
	GET_V_IFACE_ANY(GetEngineFactory, debugoverlay, CDebugOverlay, VDEBUG_OVERLAY_INTERFACE_VERSION);
#endif // WIN32

	GET_V_IFACE_ANY(GetEngineFactory, staticpropmgr, IStaticPropMgrServer, INTERFACEVERSION_STATICPROPMGR_SERVER);
	GET_V_IFACE_ANY(GetEngineFactory, enginetrace, IEngineTrace, INTERFACEVERSION_ENGINETRACE_SERVER);
	GET_V_IFACE_CURRENT(GetEngineFactory, icvar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetPhysicsFactory, iphysics, IPhysics, VPHYSICS_INTERFACE_VERSION);
	g_pCVar = icvar;
	CONVAR_REGISTER(this);
	gpGlobals = ismm->GetCGlobals();
	return true;
};

bool SDKResolveCollision::SDK_OnMetamodUnload(char* error, size_t maxlen)
{
	return true;
}

void SDKResolveCollision::SDK_OnUnload()
{
	auto safe_release = [](CDetour*& detour)
		{
			if (detour)
			{
				detour->Destroy();
				detour = nullptr;
			}
		};
	
	safe_release(g_pUpdateGroundConstraint);
	safe_release(g_pResolveCollisionDetour);
	safe_release(g_pResolveZombieCollisionDetour);
	safe_release(g_pResolveZombieClimbUpLedgeDetour);
	safe_release(g_pUpdatePosition);
}

bool SDKResolveCollision::QueryRunning(char* error, size_t maxlength)
{
	return true;
}

bool SDKResolveCollision::RegisterConCommandBase(ConCommandBase* command)
{
	return META_REGCVAR(command);
}
