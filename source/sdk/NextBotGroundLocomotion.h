//========= Copyright Valve Corporation, All rights reserved. ============//
// NextBotGroundLocomotion.h
// Basic ground-based movement for NextBotCombatCharacters
// Author: Michael Booth, February 2009
// Note: This is a refactoring of ZombieBotLocomotion from L4D

#ifndef NEXT_BOT_GROUND_LOCOMOTION_H
#define NEXT_BOT_GROUND_LOCOMOTION_H

#include "NextBotLocomotionInterface.h"

class CNavArea;
class NextBotCombatCharacter;

enum NavRelativeDirType
{
	FORWARD = 0,
	RIGHT,
	BACKWARD,
	LEFT,
	UP,
	DOWN,

	NUM_RELATIVE_DIRECTIONS
};

//----------------------------------------------------------------------------------------------------------------
/**
 * Basic ground-based movement for NextBotCombatCharacters.
 * This locomotor resolves collisions and assumes a ground-based bot under the influence of gravity.
 */
class NextBotGroundLocomotion : public ILocomotion
{
public:
	virtual ~NextBotGroundLocomotion();
		
	virtual float GetGroundAcceleration() const;
	virtual float GetYawRate() const;
	virtual void SetAcceleration(Vector const&);
	virtual void SetVelocity(Vector const&);
	virtual const Vector& GetMoveVector() const;

public:
	Vector ResolveZombieCollisions( const Vector &pos );	// push away zombies that are interpenetrating
	Vector ResolveCollision( const Vector &from, const Vector &to, int recursionLimit );	// check for collisions along move
	bool DetectCollision( trace_t *pTrace, int &nDestructionAllowed, const Vector &from, const Vector &to, const Vector &vecMins, const Vector &vecMaxs );						// return true if we are climbing a ladder
	
	void UpdatePosition(const Vector& newPos);


	bool ClimbUpToLedgeThunk(const Vector& landingGoal, const Vector& landingForward, const CBaseEntity* obstacle);
	float GetTraversableSlopeLimitThunk();

	void UpdateGroundConstraint(void);
	bool DidJustJump(void) const;

	float GetGravity() const;

	bool HasClimbingActivity();

public:
	Vector m_goal;
	Vector m_velocity;

	CBaseEntity *m_nextBot;

	Vector m_unknown;
	Vector m_lastValidPos;
	
	Vector m_acceleration;
	
	float m_desiredSpeed;									// speed bot wants to be moving
	float m_actualSpeed;									// actual speed bot is moving

	float m_maxRunSpeed;

	float m_forwardLean;
	float m_sideLean;
	QAngle m_desiredLean;
	
	bool m_isJumping;										// if true, we have jumped and have not yet hit the ground
	bool m_isJumpingAcrossGap;								// if true, we have jumped across a gap and have not yet hit the ground
	CBaseHandle m_ground;										// have to manage this ourselves, since MOVETYPE_CUSTOM always NULLs out GetGroundEntity()
	Vector m_groundNormal;									// surface normal of the ground we are in contact with
	bool m_isClimbingUpToLedge;									// true if we are jumping up to an adjacent ledge
	Vector m_ledgeJumpGoalPos;
	bool m_isUsingFullFeetTrace;							// true if we're in the air and tracing the lowest StepHeight in ResolveCollision

	const CNavLadder *m_ladder;								// ladder we are currently climbing/descending
	const CNavArea *m_ladderDismountGoal;					// the area we enter when finished with our ladder move
	bool m_isGoingUpLadder;									// if false, we're going down

	CountdownTimer m_inhibitObstacleAvoidanceTimer;			// when active, turn off path following feelers

	CountdownTimer m_wiggleTimer;							// for wiggling
	NavRelativeDirType m_wiggleDirection;

	mutable Vector m_eyePos;								// for use with GetEyes(), etc.

	Vector m_moveVector;									// the direction of our motion in XY plane
	float m_moveYaw;										// global yaw of movement direction

	Vector m_accumApproachVectors;							// weighted sum of Approach() calls since last update
	float m_accumApproachWeights;
	bool m_bRecomputePostureOnCollision;

	CountdownTimer m_ignorePhysicsPropTimer;				// if active, don't collide with physics props (because we got stuck in one)
	CBaseHandle m_ignorePhysicsProp;							// which prop to ignore
};

#endif // NEXT_BOT_GROUND_LOCOMOTION_H

