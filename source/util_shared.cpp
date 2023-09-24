#include "extension.h"
#include "util_shared.h"
#include "resolve_collision_tools.h"

CTraceFilterSimple::CTraceFilterSimple(const IHandleEntity* passedict, int collisionGroup,
	ShouldHitFunc_t pExtraShouldHitFunc)
{
	m_pPassEnt = passedict;
	m_collisionGroup = collisionGroup;
	m_pExtraShouldHitCheckFunction = pExtraShouldHitFunc;
}

//-----------------------------------------------------------------------------
// The trace filter!
//-----------------------------------------------------------------------------
bool CTraceFilterSimple::ShouldHitEntity(IHandleEntity* pHandleEntity, int contentsMask)
{
	return collisiontools->CTraceFilterSimple_ShouldHitEntity(this, pHandleEntity, contentsMask);
}