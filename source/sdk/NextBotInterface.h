#ifndef _INCLUDE_NEXTBOT_INTERFACE_H
#define _INCLUDE_NEXTBOT_INTERFACE_H

#include "NextBotEventResponderInterface.h"
#include "NextBotComponentInterface.h"

#include "Color.h"
#include "vector.h"
#include "utlvector.h"

class CBaseEntity;
class INextBotComponent;
class IBody;
class IIntention;
class IVision;

class INextBot : public INextBotEventResponder
{
public:
	INextBot(void) = default;
	virtual ~INextBot() {};

	virtual void Reset(void) {};										// (EXTEND) reset to initial state
	virtual void Update(void) {};									// (EXTEND) update internal state
	virtual void Upkeep(void) {};									// (EXTEND) lightweight update guaranteed to occur every server tick

	virtual bool IsRemovedOnReset(void) const { return true; }	// remove this bot when the NextBot manager calls Reset

	virtual CBaseCombatCharacter* GetEntity(void) const = 0;
	virtual class NextBotCombatCharacter* GetNextBotCombatCharacter(void) const { return NULL; }

	virtual class SurvivorBot* MySurvivorBotPointer() const { return NULL; }

	// interfaces are never NULL - return base no-op interfaces at a minimum
	virtual ILocomotion* GetLocomotionInterface(void) const { return nullptr; }
	virtual IBody* GetBodyInterface(void) const { return nullptr; }
	virtual IIntention* GetIntentionInterface(void) const { return nullptr; }
	virtual IVision* GetVisionInterface(void) const { return nullptr; }

	/**
	 * Attempt to change the bot's position. Return true if successful.
	 */
	virtual bool SetPosition(const Vector& pos) { return false; }
	virtual const Vector& GetPosition(void) const { return vec3_origin;	}				// get the global position of the bot

	/**
	 * Friend/enemy/neutral queries
	 */
	virtual bool IsEnemy(const CBaseEntity* them) const { return false; }			// return true if given entity is our enemy
	virtual bool IsFriend(const CBaseEntity* them) const { return false; } // return true if given entity is our friend
	virtual bool IsSelf(const CBaseEntity* them) const { return false; } 			// return true if 'them' is actually me


	virtual bool IsAllowedToClimb() const { return false; }

	/**
	 * Can we climb onto this entity?
	 */
	virtual bool IsAbleToClimbOnto(const CBaseEntity* object) const { return false; }

	/**
	 * Can we break this entity?
	 */
	virtual bool IsAbleToBreak(const CBaseEntity* object) const { return false; }

	/**
	 * Sometimes we want to pass through other NextBots. OnContact() will always
	 * be invoked, but collision resolution can be skipped if this
	 * method returns false.
	 */
	virtual bool IsAbleToBlockMovementOf(const INextBot* botInMotion) const { return true; }

	/**
	 * Should we ever care about noticing physical contact with this entity?
	 */
	virtual bool ShouldTouch(const CBaseEntity* object) const { return true; }

	virtual void ReactToSurvivorVisibility() const {}
	virtual void ReactToSurvivorNoise() const {}
	virtual void ReactToSurvivorContact() const {}

	/**
	 * This immobile system is used to track the global state of "am I actually moving or not".
	 * The OnStuck() event is only emitted when following a path, and paths can be recomputed, etc.
	 */
	virtual bool IsImmobile(void) const { return false; }					// return true if we haven't moved in awhile
	virtual float GetImmobileDuration(void) const { return 0.0f; }		// how long have we been immobile
	virtual void ClearImmobileStatus(void) {}
	virtual float GetImmobileSpeedThreshold(void) const { return 0.0f; }	// return units/second below which this actor is considered "immobile"

	// between distance utility methods
	virtual bool IsRangeLessThan(CBaseEntity* subject, float range) const { return false; }
	virtual bool IsRangeLessThan(const Vector& pos, float range) const { return false; }
	virtual bool IsRangeGreaterThan(CBaseEntity* subject, float range) const { return false; }
	virtual bool IsRangeGreaterThan(const Vector& pos, float range) const { return false; }
	virtual float GetRangeTo(CBaseEntity* subject) const { return 0.0f; }
	virtual float GetRangeTo(const Vector& pos) const { return 0.0f; }
	virtual float GetRangeSquaredTo(CBaseEntity* subject) const { return 0.0f; }
	virtual float GetRangeSquaredTo(const Vector& pos) const { return 0.0f; }

	virtual float Get2DRangeTo(Vector const&) const { return 0.0f; }
	virtual float Get2DRangeTo(CBaseEntity*) const { return 0.0f; }

	virtual const char* GetDebugIdentifier(void) const { return nullptr; }		// return the name of this bot for debugging purposes
	virtual bool IsDebugFilterMatch(const char* name) const { return false; }	// return true if we match the given debug symbol
	virtual void DisplayDebugText(const char* text) const {};	// show a line of text on the bot in the world

private:
	friend class INextBotComponent;
};

#endif // !_INCLUDE_NEXTBOT_INTERFACE_H
