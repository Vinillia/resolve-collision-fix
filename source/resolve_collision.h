
#include "resolve_collision_tools.h"
#include "util_shared.h"
#include "NextBotGroundLocomotion.h"
#include "NextBotInterface.h"
#include "NextBotBodyInterface.h"

#include <../extensions/sdkhooks/takedamageinfohack.h>

class CBaseEntity
{
public:
	virtual ~CBaseEntity() {};
};

class CPhysicsProp : public CBaseEntity
{
public:
	virtual ~CPhysicsProp() {};
};

class CBasePropDoor : public CBaseEntity
{
public:
	virtual ~CBasePropDoor() {};
};

//--------------------------------------------------------------------------------------------
/**
 * Trace filter that skips "traversable" entities.  The "when" argument creates
 * a temporal context for asking if an entity is IMMEDIATELY traversable (like thin
 * glass that just breaks as we walk through it) or EVENTUALLY traversable (like a
 * breakable object that will take some time to break through)
 */
class NextBotTraversableTraceFilter : public CTraceFilterSimple
{
public:
	NextBotTraversableTraceFilter(INextBot* bot, ILocomotion::TraverseWhenType when = ILocomotion::EVENTUALLY) : CTraceFilterSimple((IHandleEntity*)bot->GetEntity(), COLLISION_GROUP_NONE)
	{
		m_bot = bot;
		m_when = when;
	}

	virtual bool ShouldHitEntity(IHandleEntity* pServerEntity, int contentsMask)
	{
		CBaseEntity* entity = EntityFromEntityHandle(pServerEntity);

		if (m_bot->IsSelf(entity))
		{
			return false;
		}
		
		if (CTraceFilterSimple::ShouldHitEntity(pServerEntity, contentsMask))
		{
			return !m_bot->GetLocomotionInterface()->IsEntityTraversable(entity, m_when);
		}

		return false;
	}

private:
	INextBot* m_bot;
	ILocomotion::TraverseWhenType m_when;
};


class GroundLocomotionCollisionTraceFilter : public CTraceFilterSimple
{
public:
	GroundLocomotionCollisionTraceFilter(INextBot* me, const IHandleEntity* passentity, int collisionGroup) : CTraceFilterSimple(passentity, collisionGroup)
	{
		m_me = me;
	}

	virtual bool ShouldHitEntity(IHandleEntity* pServerEntity, int contentsMask)
	{
		return collisiontools->ZombieBotCollisionTraceFilter_ShouldHitEntity(this, pServerEntity, contentsMask);
	}

	virtual TraceType_t GetTraceType() const override
	{
		return TRACE_EVERYTHING;
	}

	INextBot* m_me;
};

bool NextBotGroundLocomotion::DetectCollision(trace_t* pTrace, int& recursionLimit, const Vector& from, const Vector& to, const Vector& vecMins, const Vector& vecMaxs)
{
	IBody* body = GetBot()->GetBodyInterface();
	
	CBaseEntity* ignore = m_ignorePhysicsPropTimer.IsElapsed() ? NULL : collisiontools->BaseHandleToBaseEntity(m_ignorePhysicsProp);
	GroundLocomotionCollisionTraceFilter filter(GetBot(), (IHandleEntity*)ignore, COLLISION_GROUP_NONE);
	
	TraceHull(from, to, vecMins, vecMaxs, body->GetSolidMask(), &filter, pTrace);

	if (!pTrace->DidHit())
	{
		if (z_resolve_collision_debug.GetBool())
			NDebugOverlay::SweptBox(from, to, vecMins, vecMaxs, vec3_angle, 255, 255, 255, 255, 0.1f);
		
		return false;
	}

	if (z_resolve_collision_debug.GetBool())
		NDebugOverlay::SweptBox(from, to, vecMins, vecMaxs, vec3_angle, 255, 25, 25, 255, 0.1f);

	//
	// A collision occurred - resolve it
	//
	
	// bust through "flimsy" breakables and keep on going
	if (collisiontools->DidHitNonWorldEntity(pTrace) && pTrace->m_pEnt != NULL)
	{
		CBaseEntity* other = pTrace->m_pEnt;
	
		if (!collisiontools->MyCombatCharacterPointer(other) && IsEntityTraversable(other, IMMEDIATELY) /*&& IsFlimsy( other )*/)
		{
			if (recursionLimit <= 0)
				return true;
	
			--recursionLimit;
	
			// break the weak breakable we collided with
			CTakeDamageInfoHack damageInfo((CBaseEntity*)GetBot()->GetEntity(), (CBaseEntity*)GetBot()->GetEntity(), 100.0f, DMG_CRUSH, nullptr, vec3_origin, vec3_origin);
			collisiontools->CalculateExplosiveDamageForce(&damageInfo, GetMotionVector(), pTrace->endpos);
			collisiontools->TakeDamage(other, damageInfo);
	
			// retry trace now that the breakable is out of the way
			return DetectCollision(pTrace, recursionLimit, from, to, vecMins, vecMaxs);
		}
	}
	
	/// @todo Only invoke OnContact() and Touch() once per collision pair
	// inform other components of collision
	if (GetBot()->ShouldTouch(pTrace->m_pEnt))
	{
		GetBot()->OnContact(pTrace->m_pEnt, pTrace);
	}
	
	INextBot* them = dynamic_cast<INextBot*>(pTrace->m_pEnt);
	if (them && them->ShouldTouch((CBaseEntity*)m_nextBot))
	{
		them->OnContact((CBaseEntity*)m_nextBot, pTrace);
	}
	else
	{
		collisiontools->CBaseEntity_Touch(pTrace->m_pEnt, (CBaseEntity*)GetBot()->GetEntity());
	}
	
	return true;
}

Vector NextBotGroundLocomotion::ResolveCollision(const Vector& from, const Vector& to, int recursionLimit)
{
	IBody* body = GetBot()->GetBodyInterface();
	if (body == NULL || recursionLimit < 0)
	{
		Assert(!m_bRecomputePostureOnCollision);
		return to;
	}

	// Only bother to recompute posture if we're currently standing or crouching
	if (m_bRecomputePostureOnCollision)
	{
		if (!body->IsActualPosture(IBody::STAND) && !body->IsActualPosture(IBody::CROUCH))
		{
			m_bRecomputePostureOnCollision = false;
		}
	}

	if (z_resolve_collision_debug.GetInt() == 2)
		debugoverlay->ClearAllOverlays();

	// get bounding limits, ignoring step-upable height
	bool bPerformCrouchTest = false;
	Vector mins;
	Vector maxs;
	if (m_isUsingFullFeetTrace)
	{
		mins = body->GetHullMins();
	}
	else
	{
		mins = body->GetHullMins() + Vector(0, 0, GetStepHeight());
	}
	if (!m_bRecomputePostureOnCollision)
	{
		maxs = body->GetHullMaxs();
		if (mins.z >= maxs.z)
		{
			// if mins.z is greater than maxs.z, the engine will Assert 
			// in UTIL_TraceHull, and it won't work as advertised.
			mins.z = maxs.z - 2.0f;
		}
	}
	else
	{
		const float halfSize = body->GetHullWidth() / 2.0f;
		maxs.Init(halfSize, halfSize, body->GetStandHullHeight());
		bPerformCrouchTest = true;
	}

	if (z_resolve_collision_debug.GetBool())
	{
		Vector dir = to - from;
		dir.NormalizeInPlace();
		NDebugOverlay::HorzArrow(from, from + dir * 18.0f, 5.0f, 25, 0, 255, 255, true, 0.1f);
	}

	trace_t trace;
	Vector desiredGoal = to;
	Vector resolvedGoal;
	IBody::PostureType nPosture = IBody::STAND;

	while (true)
	{
		bool bCollided = DetectCollision(&trace, recursionLimit, from, desiredGoal, mins, maxs);
		if (!bCollided)
		{
			resolvedGoal = desiredGoal;
			break;
		}

		// If we hit really close to our target, then stop
		if (z_resolve_collision.GetInt() == 2 && !trace.startsolid && desiredGoal.DistToSqr(trace.endpos) < 1.0f)
		{
			resolvedGoal = trace.endpos;
			break;
		}

		// Check for crouch test, if it's necessary
		// Don't bother about checking for crouch if we hit an actor 
		// Also don't bother checking for crouch if we hit a plane that pushes us upwards 
		if (bPerformCrouchTest)
		{
			// Don't do this work twice
			bPerformCrouchTest = false;

			nPosture = body->GetDesiredPosture();

			if (!collisiontools->MyNextBotPointer(trace.m_pEnt) && !collisiontools->CBaseEntity_IsPlayer(trace.m_pEnt))
			{
				// Here, our standing trace hit the world or something non-breakable
				// If we're not currently crouching, then see if we could travel
				// the entire distance if we were crouched
				if (nPosture != IBody::CROUCH)
				{
					trace_t crouchTrace;
					NextBotTraversableTraceFilter crouchFilter(GetBot(), ILocomotion::IMMEDIATELY);
					Vector vecCrouchMax(maxs.x, maxs.y, body->GetCrouchHullHeight());
					TraceHull(from, desiredGoal, mins, vecCrouchMax, body->GetSolidMask(), &crouchFilter, &crouchTrace);
					if (crouchTrace.fraction >= 1.0f && !crouchTrace.startsolid)
					{
						nPosture = IBody::CROUCH;
					}
				}
			}
			else if (nPosture == IBody::CROUCH)
			{
				// Here, our standing trace hit an actor

				// NOTE: This test occurs almost never, based on my tests
				// Converts from crouch to stand in the case where the player
				// is currently crouching, *and* his first trace (with the standing hull)
				// hits an actor *and* if he didn't hit that actor, he could have
				// moved standing the entire way to his desired endpoint
				trace_t standTrace;
				NextBotTraversableTraceFilter standFilter(GetBot(), ILocomotion::IMMEDIATELY);
				TraceHull(from, desiredGoal, mins, maxs, body->GetSolidMask(), &standFilter, &standTrace);
				if (standTrace.fraction >= 1.0f && !standTrace.startsolid)
				{
					nPosture = IBody::STAND;
				}
			}

			// Our first trace was based on the standing hull.
			// If we need be crouched, the trace was bogus; we need to do another
			if (nPosture == IBody::CROUCH)
			{
				maxs.z = body->GetCrouchHullHeight();
				continue;
			}
		}

		if (trace.startsolid)
		{
			// stuck inside solid; don't move

			if (trace.m_pEnt && !collisiontools->IsWorld(trace.m_pEnt))
			{
				// only ignore physics props that are not doors
				if (dynamic_cast<CPhysicsProp*>(trace.m_pEnt) != NULL && dynamic_cast<CBasePropDoor*>(trace.m_pEnt) == NULL)
				{
					IPhysicsObject* physics = collisiontools->GetPhysicsObject(trace.m_pEnt);
					if (physics && physics->IsMoveable())
					{
						// we've intersected a (likely moving) physics prop - ignore it for awhile so we can move out of it
						m_ignorePhysicsProp = ((IHandleEntity*)trace.m_pEnt)->GetRefEHandle();
						m_ignorePhysicsPropTimer.Start(1.0f);
					}
				}
			}

			// return to last known non-interpenetrating position
			resolvedGoal = m_lastValidPos;
			break;
		}

		if (--recursionLimit <= 0)
		{
			// reached recursion limit, no more adjusting allowed
			resolvedGoal = trace.endpos;
			break;
		}

		// never slide downwards/concave to avoid getting stuck in the ground
		if (trace.plane.normal.z < 0.0f)
		{
			trace.plane.normal.z = 0.0f;
			trace.plane.normal.NormalizeInPlace();
		}

		// slide off of surface we hit
		Vector fullMove = desiredGoal - from;
		Vector leftToMove = fullMove * (1.0f - trace.fraction);

		// obey climbing slope limit
		if (!body->HasActivityType(IBody::MOTION_CONTROLLED_Z) &&
			trace.plane.normal.z < GetTraversableSlopeLimit() &&
			fullMove.z > 0.0f)
		{
			fullMove.z = 0.0f;
			trace.plane.normal.z = 0.0f;
			trace.plane.normal.NormalizeInPlace();
		}

		float blocked = DotProduct(trace.plane.normal, leftToMove);

		Vector unconstrained = fullMove - blocked * trace.plane.normal;

		// check for collisions along remainder of move
		// But don't bother if we're not going to deflect much
		Vector remainingMove = from + unconstrained;
		if (z_resolve_collision.GetInt() == 2 && remainingMove.DistToSqr(trace.endpos) < 1.0f)
		{
			resolvedGoal = trace.endpos;
			break;
		}

		desiredGoal = remainingMove;
	}

	if (z_resolve_collision_debug.GetBool())
	{
		Vector dir = to - desiredGoal;
		dir.NormalizeInPlace();
		NDebugOverlay::HorzArrow(from, from + dir * 36.0f, 5.0f, 25, 255, 0, 255, true, 0.1f);
	}

	if (!trace.startsolid)
	{
		m_lastValidPos = resolvedGoal;
	}

	if (m_bRecomputePostureOnCollision)
	{
		m_bRecomputePostureOnCollision = false;

		if (!body->IsActualPosture(nPosture))
		{
			body->SetDesiredPosture(nPosture);
		}
	}

	return resolvedGoal;
}

Vector NextBotGroundLocomotion::ResolveZombieCollisions(const Vector& pos)
{
	Vector adjustedNewPos = pos;

	CBaseEntity* me = collisiontools->MyInfectedPointer(m_nextBot);
	const float hullWidth = GetBot()->GetBodyInterface()->GetHullWidth();
	const float dt = GetUpdateInterval();
	const float mul = z_resolve_zombie_collision_multiplier.GetFloat();

	// only avoid if we're actually trying to move somewhere, and are enraged
	if (me != NULL && !IsUsingLadder() && !IsClimbingOrJumping() && IsOnGround() && collisiontools->CBaseEntity_IsAlive(m_nextBot) && IsAttemptingToMove() /*&& GetBot()->GetBodyInterface()->IsArousal( IBody::INTENSE )*/)
	{
		const CUtlVector< CHandle< CBaseEntity > >& neighbors = collisiontools->Infected_GetNeighbors(me);
		Vector avoid = vec3_origin;
		float avoidWeight = 0.0f;
	
		FOR_EACH_VEC(neighbors, it)
		{
			CBaseEntity* them = gamehelpers->ReferenceToEntity(neighbors[it].GetEntryIndex());

			if (them)
			{
				Vector toThem = collisiontools->CBaseEntity_GetAbsOrigin(them) - collisiontools->CBaseEntity_GetAbsOrigin(me);
				toThem.z = 0.0f;
	
				float range = toThem.NormalizeInPlace();
	
				if (range < hullWidth)
				{
					// these two infected are in contact
					collisiontools->CBaseEntity_Touch(me, them);
					
					// move out of contact
					float penetration = hullWidth - range;
					float weight = 1.0f + (2.0f * penetration / hullWidth);
					avoid += -weight * toThem;
					avoidWeight += weight;
				}
			}
		}
	
		if (avoidWeight > 0.0f)
			adjustedNewPos += mul * (avoid / avoidWeight);
	}

	return adjustedNewPos;
}
