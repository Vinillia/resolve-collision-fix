
#include "resolve_collision_tools.h"
#include "util_shared.h"
#include "NextBotGroundLocomotion.h"
#include "NextBotInterface.h"
#include "NextBotBodyInterface.h"

#include <../extensions/sdkhooks/takedamageinfohack.h>
#include <unordered_map>

struct NextBotGroundCollisionData
{
	bool is_climbing = false;
	CountdownTimer nofall_timer;
	CountdownTimer slope_timer;
};

std::unordered_map<NextBotGroundLocomotion*, NextBotGroundCollisionData> g_nextbot_collision_data;

extern ConVar z_resolve_zombie_collision_auto_multiplier;

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

class CBreakableProp : public CBaseEntity
{
public:
	virtual ~CBreakableProp() {};
};

bool IgnoreActorsTraceFilterFunction(IHandleEntity* pServerEntity, int contentsMask)
{
	CBaseEntity* entity = EntityFromEntityHandle(pServerEntity);
	return (collisiontools->MyInfectedPointer(entity) == NULL && !collisiontools->CBaseEntity_IsPlayer(entity));
}

inline void GetClimbActivity(float height, float& heightAdjust, int& activity);

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

class NextBotTraversableTraceIgnoreActorsFilter : public NextBotTraversableTraceFilter
{
public:
	NextBotTraversableTraceIgnoreActorsFilter(INextBot* bot, ILocomotion::TraverseWhenType when = ILocomotion::EVENTUALLY) : NextBotTraversableTraceFilter(bot, when)
	{
	}

	virtual bool ShouldHitEntity(IHandleEntity* pServerEntity, int contentsMask)
	{
		if (NextBotTraversableTraceFilter::ShouldHitEntity(pServerEntity, contentsMask))
		{
			return IgnoreActorsTraceFilterFunction(pServerEntity, contentsMask);
		}

		return false;
	}
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

class NextBotTraceFilterIgnoreActors : public CTraceFilterSimple
{
public:
	NextBotTraceFilterIgnoreActors(const IHandleEntity* passentity, int collisionGroup) : CTraceFilterSimple(passentity, collisionGroup, IgnoreActorsTraceFilterFunction)
	{
	}
};

bool IsFlimsy(CBaseEntity* entity)
{
	static int m_collisionGroupOffs = collisiontools->GetDataOffset("CBaseEntity", "m_CollisionGroup");
	static int m_iHealthOffs = collisiontools->GetDataOffset(entity, "m_iHealth");
	static int m_takedamageOffs = collisiontools->GetDataOffset(entity, "m_takedamage");

	AssertOnce(m_collisionGroupOffs != -1 && m_iHealthOffs != -1 && m_takedamageOffs != -1);

	const Collision_Group_t& collisionGroup = *reinterpret_cast<Collision_Group_t*>((uintptr_t)entity + m_collisionGroupOffs);
	const int& m_iHealth = *reinterpret_cast<int*>((uintptr_t)entity + m_iHealthOffs);
	const int8_t& m_takedamage = *reinterpret_cast<int8_t*>((uintptr_t)entity + m_takedamageOffs);

	if ((collisionGroup == COLLISION_GROUP_BREAKABLE_GLASS || collisionGroup == COLLISION_GROUP_NONE) &&
		m_takedamage == 2 &&
		m_iHealth <= 10 &&
		(ClassMatchesComplex(entity, "func_breakable_surf") ||
			ClassMatchesComplex(entity, "func_breakable") ||
			dynamic_cast<CBreakableProp*>(entity) != nullptr))
	{
		return true;
	}

	return false;
}

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
	
		if (!collisiontools->MyCombatCharacterPointer(other) && IsEntityTraversable(other, IMMEDIATELY) && IsFlimsy( other ))
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

	if (g_nextbot_collision_data[this].is_climbing && !HasClimbingActivity())
	{
		g_nextbot_collision_data[this].is_climbing = false;
		g_nextbot_collision_data[this].nofall_timer.Start(0.1f);
		g_nextbot_collision_data[this].slope_timer.Start(0.16f);
	}

	CountdownTimer& slope = g_nextbot_collision_data[this].nofall_timer;

	if (slope.HasStarted() && !slope.IsElapsed())
	{
		if (from.z > to.z)
			const_cast<Vector&>(to).z = from.z;
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
		m_lastValidPos = from;
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
				Vector toThem = (collisiontools->CBaseEntity_GetAbsOrigin(them) - collisiontools->CBaseEntity_GetAbsOrigin(me));
				toThem.z = 0.0f;
	
				float range = toThem.NormalizeInPlace();

				if (range < hullWidth)
				{
					// these two infected are in contact
					collisiontools->CBaseEntity_Touch(me, them);
					
					// move out of contact
					float penetration = (hullWidth - range);
					float weight = 1.0f + (2.0f * penetration / hullWidth);
					
					avoid += -weight * toThem;
					avoidWeight += weight;
				}
			}
		}
	
		if (avoidWeight > 0.0f)
		{
			Vector collision = avoid / avoidWeight;
			
			if (z_resolve_zombie_collision_auto_multiplier.GetBool())
			{
				collision *= (dt / 0.1f);
			}
			else
			{
				collision *= mul;
			}

			adjustedNewPos += collision;
		}
	}

	return adjustedNewPos;
}

bool NextBotGroundLocomotion::ClimbUpToLedgeThunk(const Vector& landingGoal, const Vector& landingForward, const CBaseEntity* obstacle)
{
	INextBot* bot = GetBot();
	IBody* body = bot->GetBodyInterface();

	if (!IsOnGround() || m_isClimbingUpToLedge)
		return false;

	const Vector& feet = GetFeet();
	const float height = landingGoal.z - feet.z;

	float heightFixed;
	int activity;
	GetClimbActivity(height, heightFixed, activity);

	Vector mins(-1.0f, -1.0f, GetStepHeight());
	Vector maxs(1.0f, 1.0f, height);

	if (mins.z > height)
	{
		maxs.z = mins.z;
		mins.z = height;
	}

	float heightAdjust = 0.0f;
	float hullWidth = body->GetHullWidth();
	float hullWidthX3 = hullWidth * 3.0f;

	trace_t trace;
	Vector start, end, normal;
	Vector landingGoalResolve = landingGoal;
	landingGoalResolve.z = feet.z;

	while (true)
	{
		start = landingGoalResolve - (landingForward * heightAdjust);
		end = feet + hullWidth * 10.0f * landingForward;

		NextBotTraversableTraceIgnoreActorsFilter filter(bot, IMMEDIATELY);
		TraceHull(start, end, mins, maxs, body->GetSolidMask(), &filter, &trace);

		normal = trace.plane.normal;
		heightAdjust = (hullWidth * 0.5) + heightAdjust;

		if (hullWidthX3 < heightAdjust)
			goto ret;

		if (trace.startsolid || (trace.fraction >= 1.0 && !trace.allsolid))
		{
			continue;
		}

		break;
	}

	if (hullWidthX3 < heightAdjust || (trace.fraction >= 1.0 && !trace.allsolid))
	{
	ret:
		m_velocity = vec3_origin;
		m_acceleration = vec3_origin;
		return false;
	}

	heightAdjust = 0.0f;
	normal.z = 0.0f;
	VectorNormalize(normal);

	while (true)
	{
		start = normal * heightAdjust + landingGoalResolve;

		end = start;
		end.z = landingGoal.z;

		NextBotTraversableTraceIgnoreActorsFilter filter(bot);
		TraceHull(start, end, body->GetHullMins(), body->GetHullMaxs(), body->GetSolidMask(), &filter, &trace);

		if (trace.fraction >= 1.0 && !trace.allsolid && !trace.startsolid)
			break;

		heightAdjust = (hullWidth * 0.25f) + heightAdjust;
		if (hullWidthX3 < heightAdjust)
		{
			goto label_61;
		}
	}

	landingGoalResolve = normal * heightAdjust + landingGoalResolve;

label_61:
	Vector goal, inverseNormal = -normal;
	QAngle inverseAngle;

	goal = landingGoalResolve;
	goal.z += heightFixed;

	VectorAngles(inverseNormal, inverseAngle);
	DriveTo(goal);
	collisiontools->CBaseEntity_SetAbsAngles(m_nextBot, inverseAngle);

	if (!body->StartActivity(activity, IBody::ACTIVITY_TRANSITORY | IBody::MOTION_CONTROLLED_XY | IBody::MOTION_CONTROLLED_Z))
		return false;

	if (z_resolve_zombie_climb_up_ledge_debug.GetBool())
	{
		NDebugOverlay::Line(landingGoal, goal, 255, 0, 0, true, 15.0f);
		NDebugOverlay::Line(GetFeet(), landingGoal, 0, 255, 0, true, 15.0f);
		NDebugOverlay::Line(GetFeet(), landingGoalResolve , 0, 0, 255, true, 15.0f);
	}

	m_isJumping = true;
	m_isClimbingUpToLedge = true;
	m_ledgeJumpGoalPos = landingGoal;
	body->SetDesiredPosture(IBody::CROUCH);
	bot->OnLeaveGround(GetGround());

	g_nextbot_collision_data[this].is_climbing = true;
	return true;
}

void NextBotGroundLocomotion::UpdateGroundConstraint(void)
{
	// if we're up on the upward arc of our jump, don't interfere by snapping to ground
	// don't do ground constraint if we're climbing a ladder
	if (DidJustJump() || IsAscendingOrDescendingLadder())
	{
		m_isUsingFullFeetTrace = false;
		return;
	}

	IBody* body = GetBot()->GetBodyInterface();
	if (body == NULL)
	{
		return;
	}

	float halfWidth = body->GetHullWidth() / 2.0f;

	// since we only care about ground collisions, keep hull short to avoid issues with low ceilings
	/// @TODO: We need to also check actual hull height to avoid interpenetrating the world
	float hullHeight = GetStepHeight();

	// always need tolerance even when jumping/falling to make sure we detect ground penetration
	// must be at least step height to avoid 'falling' down stairs
	const float stickToGroundTolerance = GetStepHeight() + 0.01f;

	trace_t ground;
	NextBotTraceFilterIgnoreActors filter((IHandleEntity*)m_nextBot, COLLISION_GROUP_NONE);

	TraceHull(GetBot()->GetPosition() + Vector(0, 0, GetStepHeight() + 0.001f),
		GetBot()->GetPosition() + Vector(0, 0, -stickToGroundTolerance),
		Vector(-halfWidth, -halfWidth, 0),
		Vector(halfWidth, halfWidth, hullHeight),
		body->GetSolidMask(), &filter, &ground);

	if (ground.startsolid)
		return;

	if (ground.fraction < 1.0f)
	{
		// there is ground below us
		m_groundNormal = ground.plane.normal;

		m_isUsingFullFeetTrace = false;

		// zero velocity normal to the ground
		float normalVel = DotProduct(m_groundNormal, m_velocity);
		m_velocity -= normalVel * m_groundNormal;

		// check slope limit
		if (ground.plane.normal.z < GetTraversableSlopeLimitThunk())
		{
			// too steep to stand here

			// too steep to be ground - treat it like a wall hit
			if ((m_velocity.x * ground.plane.normal.x + m_velocity.y * ground.plane.normal.y) <= 0.0f)
			{
				GetBot()->OnContact(ground.m_pEnt, &ground);
			}

			// we're contacting some kind of ground
			// zero accelerations normal to the ground

			float normalAccel = DotProduct(m_groundNormal, m_acceleration);
			m_acceleration -= normalAccel * m_groundNormal;

			// clear out upward velocity so we don't walk up lightpoles
			m_velocity.z = MIN(0, m_velocity.z);
			m_acceleration.z = MIN(0, m_acceleration.z);

			return;
		}

		// inform other components of collision if we didn't land on the 'world'
		if (ground.m_pEnt && !collisiontools->IsWorld(ground.m_pEnt))
		{
			GetBot()->OnContact(ground.m_pEnt, &ground);
		}

		// snap us to the ground 
		GetBot()->SetPosition(ground.endpos);

		if (!IsOnGround())
		{
			// just landed
			collisiontools->CBaseEntity_SetGroundEntity(m_nextBot, ground.m_pEnt);
			m_ground = ground.m_pEnt != nullptr ? ((IHandleEntity*)ground.m_pEnt)->GetRefEHandle() : -1;

			// landing stops any jump in progress
			m_isJumping = false;
			m_isJumpingAcrossGap = false;

			GetBot()->OnLandOnGround(ground.m_pEnt);
		}
	}
	else
	{
		// not on the ground
		if (IsOnGround())
		{
			GetBot()->OnLeaveGround(GetGround());
			if (!IsClimbingUpToLedge() && !IsJumpingAcrossGap())
			{
				m_isUsingFullFeetTrace = true; // We're in the air and there's space below us, so use the full trace
				m_acceleration.z -= GetGravity(); // start our gravity now
			}
		}
	}
}

bool NextBotGroundLocomotion::DidJustJump(void) const
{
	const Vector& velocity = collisiontools->CBaseEntity_GetAbsVelocity(m_nextBot);
	return IsClimbingOrJumping() && (velocity.z > 0.0f);
}

float NextBotGroundLocomotion::GetTraversableSlopeLimitThunk()
{
	CountdownTimer& slopeTimer = g_nextbot_collision_data[this].slope_timer;
	float actualSlope = GetTraversableSlopeLimit();

	if (slopeTimer.HasStarted())
	{
		if (slopeTimer.IsElapsed())
		{
			slopeTimer.Invalidate();
			return actualSlope;
		}

		return 0.0f;
	}

	return actualSlope;
}

inline float NextBotGroundLocomotion::GetGravity() const
{
	static ConVarRef nb_gravity("nb_gravity", false);

	if (nb_gravity.IsValid())
		return nb_gravity.GetFloat();

	return 1000.0f;
}

inline bool NextBotGroundLocomotion::HasClimbingActivity()
{
	IBody* body = GetBot()->GetBodyInterface();

	if (!body)
		return false;

	Activity activity = body->GetActivity();

	switch (activity)
	{
#ifdef WIN32
		case 737:
		case 735:
		case 733:
		case 732:
		case 730:
		case 728:
		case 727:
		case 726:
		case 725:
		case 723:
		case 721:
		case 719:
		case 718:
#else
		case 718:
		case 721:
		case 737:
		case 735:
		case 733:
		case 732:
		case 730:
		case 728:
		case 727:
		case 726:
		case 725:
		case 723:
		case 719:
#endif
			return true;
	default:
		break;
	}

	return false;
}

#ifdef WIN32
inline void GetClimbActivity(float height, float& heightAdjust, int& activity)
{
	if (height >= 30.0)
	{
		if (height >= 42.0)
		{
			if (height >= 54.0)
			{
				if (height >= 66.0)
				{
					if (height >= 78.0)
					{
						if (height >= 90.0)
						{
							if (height >= 102.0)
							{
								if (height >= 114.0)
								{
									if (height >= 126.0)
									{
										if (height >= 138.0)
										{
											if (height >= 150.0)
											{
												if (height >= 162.0)
												{
													heightAdjust = height - 168.0;
													activity = 737;
												}
												else
												{
													heightAdjust = height - 156.0;
													activity = 735;
												}
											}
											else
											{
												heightAdjust = height - 144.0;
												activity = 733;
											}
										}
										else
										{
											heightAdjust = height - 132.0;
											activity = 732;
										}
									}
									else
									{
										heightAdjust = height - 120.0;
										activity = 730;
									}
								}
								else
								{
									heightAdjust = height - 108.0;
									activity = 728;
								}
							}
							else
							{
								heightAdjust = height - 96.0;
								activity = 727;
							}
						}
						else
						{
							heightAdjust = height - 84.0;
							activity = 726;
						}
					}
					else
					{
						heightAdjust = height - 72.0;
						activity = 725;
					}
				}
				else
				{
					heightAdjust = height - 60.0;
					activity = 723;
				}
			}
			else
			{
				heightAdjust = height - 48.0;
				activity = 721;
			}
		}
		else
		{
			heightAdjust = height - 36.0;
			activity = 719;
		}
	}
	else
	{
		heightAdjust = height - 24.0;
		activity = 718;
	}
}
#else
inline void GetClimbActivity(float height, float& heightAdjust, int& activity)
{
	if (height < 30.0)
	{
		activity = 718;
		heightAdjust = height - 24.0;
	}
	else if (height >= 42.0)
	{
		if (height < 54.0)
		{
			activity = 721;
			heightAdjust = height - 48.0;
		}
		else if (height >= 66.0)
		{
			if (height >= 78.0)
			{
				if (height >= 90.0)
				{
					if (height >= 102.0)
					{
						if (height >= 114.0)
						{
							if (height >= 126.0)
							{
								if (height >= 138.0)
								{
									if (height >= 150.0)
									{
										if (height >= 162.0)
										{
											heightAdjust = height - 168.0;
											activity = 737;
										}
										else
										{
											heightAdjust = height - 156.0;
											activity = 735;
										}
									}
									else
									{
										activity = 733;
										heightAdjust = height - 144.0;
									}
								}
								else
								{
									activity = 732;
									heightAdjust = height - 132.0;
								}
							}
							else
							{
								activity = 730;
								heightAdjust = height - 120.0;
							}
						}
						else
						{
							activity = 728;
							heightAdjust = height - 108.0;
						}
					}
					else
					{
						activity = 727;
						heightAdjust = height - 96.0;
					}
				}
				else
				{
					activity = 726;
					heightAdjust = height - 84.0;
				}
			}
			else
			{
				activity = 725;
				heightAdjust = height - 72.0;
			}
		}
		else
		{
			activity = 723;
			heightAdjust = height - 60.0;
		}
	}
	else
	{
		activity = 719;
		heightAdjust = height - 36.0;
	}
}
#endif

