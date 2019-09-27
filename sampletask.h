#pragma once

class SampleTask
{
public:
	SampleTask(long expiration)
		: expiration_(expiration)
	{
	}

	long GetExpiration()
	{
		return expiration_;
	}

	void *GetTimerTaskEntry()
	{
		return entry_;
	}
	void SetTimerTaskEntry(void *entry)
	{
		this->entry_ = entry;
	}

	static void Handler(SampleTask *task)
	{
		//TODO
	}

private:
	long expiration_;
	void *entry_;
};