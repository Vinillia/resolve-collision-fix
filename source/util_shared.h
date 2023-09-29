//-----------------------------------------------------------------------------
// Converts an IHandleEntity to an CBaseEntity
//-----------------------------------------------------------------------------
inline const CBaseEntity* EntityFromEntityHandle(const IHandleEntity* pConstHandleEntity)
{
	IHandleEntity* pHandleEntity = const_cast<IHandleEntity*>(pConstHandleEntity);

	if (staticpropmgr->IsStaticProp(pHandleEntity))
		return NULL;

	IServerUnknown* pUnk = (IServerUnknown*)pHandleEntity;
	return pUnk->GetBaseEntity();
}

inline CBaseEntity* EntityFromEntityHandle(IHandleEntity* pHandleEntity)
{
	if (staticpropmgr->IsStaticProp(pHandleEntity))
		return NULL;

	IServerUnknown* pUnk = (IServerUnknown*)pHandleEntity;
	return pUnk->GetBaseEntity();
}

typedef bool (*ShouldHitFunc_t)(IHandleEntity* pHandleEntity, int contentsMask);

class CTraceFilterSimple : public CTraceFilter
{
public:
	CTraceFilterSimple(const IHandleEntity* passentity, int collisionGroup, ShouldHitFunc_t pExtraShouldHitCheckFn = NULL);
	virtual bool ShouldHitEntity(IHandleEntity* pHandleEntity, int contentsMask);
	virtual void SetPassEntity(const IHandleEntity* pPassEntity) { m_pPassEnt = pPassEntity; }
	virtual void SetCollisionGroup(int iCollisionGroup) { m_collisionGroup = iCollisionGroup; }

	const IHandleEntity* GetPassEntity(void) { return m_pPassEnt; }

private:
	const IHandleEntity* m_pPassEnt;
	int m_collisionGroup;
	ShouldHitFunc_t m_pExtraShouldHitCheckFunction;
};

//--------------------------------------------------------------------------------------------------------------
/**
 * Simple class for tracking intervals of game time.
 * Upon creation, the timer is invalidated.  To measure time intervals, start the timer via Start().
 */
class IntervalTimer
{
public:
	IntervalTimer(void)
	{
		m_timestamp = -1.0f;
	}

	void Reset(void)
	{
		m_timestamp = Now();
	}

	void Start(void)
	{
		m_timestamp = Now();
	}

	void Invalidate(void)
	{
		m_timestamp = -1.0f;
	}

	bool HasStarted(void) const
	{
		return (m_timestamp > 0.0f);
	}

	/// if not started, elapsed time is very large
	float GetElapsedTime(void) const
	{
		return (HasStarted()) ? (Now() - m_timestamp) : 99999.9f;
	}

	bool IsLessThen(float duration) const
	{
		return (Now() - m_timestamp < duration) ? true : false;
	}

	bool IsGreaterThen(float duration) const
	{
		return (Now() - m_timestamp > duration) ? true : false;
	}

private:
	float m_timestamp;

	virtual float Now(void) const
	{
		return gpGlobals->curtime;
	}
};


//--------------------------------------------------------------------------------------------------------------
/**
 * Simple class for counting down a short interval of time.
 * Upon creation, the timer is invalidated.  Invalidated countdown timers are considered to have elapsed.
 */
class CountdownTimer
{
public:
	CountdownTimer(void)
	{
		m_timestamp = -1.0f;
		m_duration = 0.0f;
	}

	void Reset(void)
	{
		m_timestamp = Now() + m_duration;
	}

	void Start(float duration)
	{
		m_timestamp = Now() + duration;
		m_duration = duration;
	}

	void Invalidate(void)
	{
		m_timestamp = -1.0f;
	}

	bool HasStarted(void) const
	{
		return (m_timestamp > 0.0f);
	}

	bool IsElapsed(void) const
	{
		return (Now() > m_timestamp);
	}

	float GetElapsedTime(void) const
	{
		return Now() - m_timestamp + m_duration;
	}

	float GetRemainingTime(void) const
	{
		return (m_timestamp - Now());
	}

	/// return original countdown time
	float GetCountdownDuration(void) const
	{
		return (m_timestamp > 0.0f) ? m_duration : 0.0f;
	}

private:
	float m_duration;
	float m_timestamp;

	virtual float Now(void) const
	{
		return gpGlobals->curtime;
	}
};
