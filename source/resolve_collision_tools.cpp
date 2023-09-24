#include "resolve_collision_tools.h"
#include "extension.h"


ResolveCollisionTools g_collisionTools;
ResolveCollisionTools* collisiontools = &g_collisionTools;

ResolveCollisionTools::ResolveCollisionTools()
{
	m_CTraceFilterSimple_ShouldHitEntity = nullptr;
	m_ZombieBotCollisionTraceFilter_ShouldHitEntity = nullptr;
	m_CBaseEntity_TakeDamage = nullptr;
	m_CalculateExplosiveDamageForce = nullptr;
	m_MyCombatCharacterPointer = -1;
	m_MyNextBotPointer = -1;
	m_MyInfectedPointer = -1;
	m_CBaseEntity_Touch = -1;
	m_CBaseEntity_IsPlayer = -1;
	m_CBaseEntity_IsAlive = -1;
	m_Infected_m_vecNeighbors = -1;
	m_CBaseEntity_m_vecAbsOrigin = 0;
}

bool ResolveCollisionTools::Initialize(SourceMod::IGameConfig* config)
{
	bool ok = true;

	auto GetFunctionAddress = [&ok, config](const char* key) -> void*
	{
		void* value = nullptr;

		if (config->GetAddress(key, &value) && value)
			return value;

		if (config->GetMemSig(key, &value) && value)
			return value;

		g_pSM->LogMessage(myself, "Failed to find '%s' address", key);
		ok = false;
		return nullptr;
	};

	auto GetConfigOffset = [&ok, config](const char* key) -> int
	{
		int value = -1;
		if (config->GetOffset(key, &value))
			return value;

		g_pSM->LogMessage(myself, "Failed to find '%s' offset", key);
		ok = false;
		return -1;
	};

	m_CTraceFilterSimple_ShouldHitEntity = GetFunctionAddress("CTraceFilterSimple::ShouldHitEntity");
	m_ZombieBotCollisionTraceFilter_ShouldHitEntity = GetFunctionAddress("ZombieBotCollisionTraceFilter::ShouldHitEntity");
	m_CBaseEntity_TakeDamage = GetFunctionAddress("CBaseEntity::TakeDamage");
	m_CalculateExplosiveDamageForce = GetFunctionAddress("CalculateExplosiveDamageForce");

	m_MyCombatCharacterPointer = GetConfigOffset("MyCombatCharacterPointer");
	m_MyNextBotPointer = GetConfigOffset("MyNextBotPointer");
	m_MyInfectedPointer = GetConfigOffset("MyInfectedPointer");
	m_CBaseEntity_Touch = GetConfigOffset("CBaseEntity::Touch");
	m_CBaseEntity_IsPlayer = GetConfigOffset("CBaseEntity::IsPlayer");
	m_CBaseEntity_IsAlive = GetConfigOffset("CBaseEntity::IsAlive");
	m_CBaseEntity_m_vecAbsOrigin = GetConfigOffset("CBaseEntity::m_vecAbsOrigin");
	m_Infected_m_vecNeighbors = GetConfigOffset("Infected::m_vecNeighbors");
	
	return ok;
}

// https://github.com/asherkin/vphysics/blob/d5e0287bb11b3a06dd727e66a9f3442e693dcf58/extension/physnatives.cpp#L1034-L1055
IPhysicsObject* ResolveCollisionTools::GetPhysicsObject(CBaseEntity* pEntity)
{
	datamap_t* data = gamehelpers->GetDataMap(pEntity);

	if (!data)
	{
		return NULL;
	}

	typedescription_t* type = gamehelpers->FindInDataMap(data, "m_pPhysicsObject");

	if (!type)
	{
		return NULL;
	}

#if SOURCE_ENGINE >= SE_LEFT4DEAD
	return *(IPhysicsObject**)((char*)pEntity + type->fieldOffset);
#else
	return *(IPhysicsObject**)((char*)pEntity + type->fieldOffset[TD_OFFSET_NORMAL]);
#endif
}

CBaseEntity* ResolveCollisionTools::BaseHandleToBaseEntity(const CBaseHandle& handle)
{
	if (!handle.IsValid())
		return nullptr;

	return gamehelpers->ReferenceToEntity(handle.GetEntryIndex());
}

bool ResolveCollisionTools::IsWorld(CBaseEntity* entity)
{
	return gamehelpers->EntityToBCompatRef(entity) == 0;
}

bool ResolveCollisionTools::DidHitNonWorldEntity(CGameTrace* trace)
{
	return trace->m_pEnt != NULL && !IsWorld(trace->m_pEnt);
}
void ResolveCollisionTools::CalculateExplosiveDamageForce(CTakeDamageInfo* info, const Vector& vecDir, const Vector& vecForceOrigin, float flScale)
{
	ine::call_cdecl<void, CTakeDamageInfo*, const Vector&, const Vector&>(m_CalculateExplosiveDamageForce, info, vecDir, vecForceOrigin, flScale);
}

void ResolveCollisionTools::TakeDamage(CBaseEntity* entity, const CTakeDamageInfo& info)
{
	ine::call_this<void, CBaseEntity*, const CTakeDamageInfo&>(m_CBaseEntity_TakeDamage, entity, info);
}
