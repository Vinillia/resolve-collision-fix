#ifndef _INCLUDE_RESOLVE_COLLISION_TOOLS_H
#define _INCLUDE_RESOLVE_COLLISION_TOOLS_H

#include <utlvector.h>
#include <ehandle.h>

namespace ine
{
	template<typename K, typename... T>
	inline K call_cdecl(void* addr, T... args)
	{
		using fn_t = K(__cdecl*)(T...);
		fn_t fn = reinterpret_cast<fn_t>(addr);
		return fn(args...);
	}
	
	template<typename K, typename... T>
	inline K call_this(void* addr, T... args)
	{
#ifdef WIN32
		using fn_t = K(__thiscall*)(T...);
		fn_t fn = reinterpret_cast<fn_t>(addr);
		return fn(args...);
#else
		return call_cdecl<K, T...>(addr, args...);
#endif
	}

	inline void* get_vtable_fn(void* instance, int offset)
	{
		void** vtable = *reinterpret_cast<void***>(instance);
		return vtable[offset];
	}

	template<typename K, typename... T>
	inline K call_vtable(void* instance, int offset, T... args)
	{
		return call_this<K>(get_vtable_fn(instance, offset), instance, args...);
	}
}

namespace SourceMod
{
	class IGameConfig;
}

class IHandleEntity;
class ITraceFilter;
class CBaseEntity;
class INextBot;
class CBaseHandle;
class IPhysicsObject;
class CGameTrace;
class CTakeDamageInfo;
class Vector;

template< class T >
class CHandle;

class ResolveCollisionTools
{
	friend class SDKResolveCollision;

public:
	ResolveCollisionTools();

public:
	int GetDataOffset(CBaseEntity* entity, const char* name);
	int GetDataOffset(const char* netclass, const char* property);

	inline bool CTraceFilterSimple_ShouldHitEntity(ITraceFilter* trace, IHandleEntity* pHandleEntity, int contentsMask);
	inline bool ZombieBotCollisionTraceFilter_ShouldHitEntity(ITraceFilter* trace, IHandleEntity* pHandleEntity, int contentsMask);
	
	inline CBaseEntity* MyCombatCharacterPointer(CBaseEntity* entity);
	inline INextBot* MyNextBotPointer(CBaseEntity* entity);
	inline CBaseEntity* MyInfectedPointer(CBaseEntity* entity);

	inline void CBaseEntity_SetGroundEntity(CBaseEntity* entity, CBaseEntity* ground);
	inline void CBaseEntity_SetAbsAngles(CBaseEntity* entity, const QAngle& angle);
	inline void CBaseEntity_Touch(CBaseEntity* entity, CBaseEntity* them);
	inline bool CBaseEntity_IsPlayer(CBaseEntity* entity);
	inline bool CBaseEntity_IsAlive(CBaseEntity* entity);

	inline const Vector& CBaseEntity_GetAbsOrigin(CBaseEntity* entity);
	inline const Vector& CBaseEntity_GetAbsVelocity(CBaseEntity* entity);
	CBaseEntity* CBaseEntity_GetGroundEntity(CBaseEntity* entity);

	inline CUtlVector< CHandle< CBaseEntity > >& Infected_GetNeighbors(CBaseEntity* infected);

	CBaseEntity* BaseHandleToBaseEntity(const CBaseHandle& handle);
	bool IsWorld(CBaseEntity* entity);
	IPhysicsObject* GetPhysicsObject(CBaseEntity* pEntity);

	bool DidHitNonWorldEntity(CGameTrace* trace);
	void TakeDamage(CBaseEntity* entity, const CTakeDamageInfo& info);
	void CalculateExplosiveDamageForce(CTakeDamageInfo* info, const Vector& vecDir, const Vector& vecForceOrigin, float flScale = 1.0f);

protected:
	bool Initialize(SourceMod::IGameConfig* config);

	void* m_CTraceFilterSimple_ShouldHitEntity;
	void* m_ZombieBotCollisionTraceFilter_ShouldHitEntity;
	void* m_CBaseEntity_TakeDamage;
	void* m_CBaseEntity_SetAbsAngles;
	void* m_CBaseEntity_SetGroundEntity;
	void* m_CalculateExplosiveDamageForce;

	int m_MyCombatCharacterPointer;
	int m_MyNextBotPointer;
	int m_MyInfectedPointer;
	int m_CBaseEntity_Touch;
	int m_CBaseEntity_IsPlayer;
	int m_CBaseEntity_IsAlive;
	int m_Infected_m_vecNeighbors;
	
	int m_CBaseEntity_m_vecAbsOrigin;
	int m_CBaseEntity_m_vecAbsVelocity;
	int m_CBaseEntity_m_hGroundEntity;
};

inline bool ResolveCollisionTools::CTraceFilterSimple_ShouldHitEntity(ITraceFilter* trace, IHandleEntity* pHandleEntity, int contentsMask)
{
	return ine::call_this<bool>(m_CTraceFilterSimple_ShouldHitEntity, trace, pHandleEntity, contentsMask);
}

inline bool ResolveCollisionTools::ZombieBotCollisionTraceFilter_ShouldHitEntity(ITraceFilter* trace, IHandleEntity* pHandleEntity, int contentsMask)
{
	return ine::call_this<bool>(m_ZombieBotCollisionTraceFilter_ShouldHitEntity, trace, pHandleEntity, contentsMask);
}

inline CBaseEntity* ResolveCollisionTools::MyCombatCharacterPointer(CBaseEntity* entity)
{
	return ine::call_vtable<CBaseEntity*>(entity, m_MyCombatCharacterPointer);
}

inline INextBot* ResolveCollisionTools::MyNextBotPointer(CBaseEntity* entity)
{
	return ine::call_vtable<INextBot*>(entity, m_MyNextBotPointer);
}

inline CBaseEntity* ResolveCollisionTools::MyInfectedPointer(CBaseEntity* entity)
{
	return ine::call_vtable<CBaseEntity*>(entity, m_MyInfectedPointer);
}

inline void ResolveCollisionTools::CBaseEntity_SetAbsAngles(CBaseEntity* entity, const QAngle& angle)
{
	ine::call_this<void, CBaseEntity*, const QAngle&>(m_CBaseEntity_SetAbsAngles, entity, angle);
}

inline void ResolveCollisionTools::CBaseEntity_Touch(CBaseEntity* entity, CBaseEntity* them)
{
	ine::call_vtable<void>(entity, m_CBaseEntity_Touch, them);
}

inline bool ResolveCollisionTools::CBaseEntity_IsPlayer(CBaseEntity* entity)
{
	return ine::call_vtable<bool>(entity, m_CBaseEntity_IsPlayer);
}

inline bool ResolveCollisionTools::CBaseEntity_IsAlive(CBaseEntity* entity)
{
	return ine::call_vtable<bool>(entity, m_CBaseEntity_IsAlive);
}

inline CUtlVector<CHandle<CBaseEntity>>& ResolveCollisionTools::Infected_GetNeighbors(CBaseEntity* infected)
{
	return *reinterpret_cast<CUtlVector<CHandle<CBaseEntity>>*>((int)(infected) + m_Infected_m_vecNeighbors);
}

inline const Vector& ResolveCollisionTools::CBaseEntity_GetAbsOrigin(CBaseEntity* entity)
{
	if (m_CBaseEntity_m_vecAbsOrigin == -1)
	{
		m_CBaseEntity_m_vecAbsOrigin = GetDataOffset(entity, "m_vecAbsOrigin");
		Assert(m_CBaseEntity_m_vecAbsOrigin != -1);
	}

	return *reinterpret_cast<const Vector*>((int)(entity) + m_CBaseEntity_m_vecAbsOrigin);
}

inline const Vector& ResolveCollisionTools::CBaseEntity_GetAbsVelocity(CBaseEntity* entity)
{
	if (m_CBaseEntity_m_vecAbsVelocity == -1)
	{
		m_CBaseEntity_m_vecAbsVelocity = GetDataOffset(entity, "m_vecAbsVelocity");
		Assert(m_CBaseEntity_m_vecAbsVelocity != -1);
	}

	return *reinterpret_cast<const Vector*>((int)(entity) + m_CBaseEntity_m_vecAbsVelocity);
}

inline void ResolveCollisionTools::CBaseEntity_SetGroundEntity(CBaseEntity* entity, CBaseEntity* ground)
{
	ine::call_this<void>(m_CBaseEntity_SetGroundEntity, entity, ground);
}

extern ResolveCollisionTools* collisiontools;

#endif // !_INCLUDE_RESOLVE_COLLISION_TOOLS_H