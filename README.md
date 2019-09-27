gamewheels-timingwheel
===============
一个专为游戏服务器而设计的时间轮

# API
```c++
// 时间轮
template <typename Task> class TTimingWheel {
    typedef void (*TimerTaskHandler)(Task *);
    // 构造函数 taskHandler: 任务到期处理派发函数
    TTimingWheel(int tickMs, int wheelSize, long startMs, TimerTaskHandler taskHandler);
    // 析构函数
    ~TTimingWheel();
    // 推进时间 timeMs: 当前时间(毫秒)
    void AdvanceClock(long timeMs);
    // 返回列表中任务总数
    long GetCount();
    // 增加定时任务
    void Add(Task *timerTask);
    // 移除定时任务
    bool Remove(Task *timerTask);
    // 关机清理
    void Shutdown();
};

// 定时任务列表
template <typename Task> class TTimerTaskList {
    // 构造函数
    TTimerTaskList();
    // 析构函数
    ~TTimerTaskList();
    // 增加定时任务
    void Add(Task *timerTask);
    // 移除定时任务
    bool Remove(Task *timerTask);
    // 返回列表中任务数目
    long GetCount();

    // 清除任务
    void Clear();

    typedef void (*TimerTaskHandler)(Task *);
    typedef void (*TimerTaskTarget)(void *, Task *);

    // 处理每个任务
    void ForEach(TimerTaskHandler handler);
    // 处理每个任务
    void ForEach(TimerTaskTarget target, void *obj);
};

// 一个定时任务的示例
class SampleTask
{
	SampleTask(long expiration)；
	long GetExpiration();
	void *GetTimerTaskEntry()；
	void SetTimerTaskEntry(void *entry);
	static void Handler(SampleTask *task);
};
```

# 示例
```c++
#include <iostream>
#include <thread>
#include "timingwheel.h"

using namespace std;

class GameTask
{
public:
	GameTask(const string &name, long expiration)
		: name_(name), expiration_(expiration)
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

	const string &GetName()
	{
		return name_;
	}

	static void Handler(GameTask *task)
	{
		cout << "task name=" << task->name_ << endl;
	}

private:
	string name_;
	long expiration_;
	void *entry_;
};

int main(int argc, char *argv[])
{
	GameTask *t1 = new GameTask("game task delay 1ms", 1);
	GameTask *t5 = new GameTask("game task delay 5ms", 5);
	GameTask *t10 = new GameTask("game task delay 10ms", 10);
    GameTask *t16 = new GameTask("game task delay 16ms", 16);
    GameTask *t160 = new GameTask("game task delay 160ms", 160);

	TTimingWheel<GameTask> tw(1, 10, 0, GameTask::HandleTask);

	tw.Add(t1);
	tw.Add(t5);
	tw.Add(t10);
	tw.Add(t16);
	tw.Add(t160);

	std::chrono::milliseconds ms(1);

	for (int i = 0; i < 200; ++i)
	{
		std::this_thread::sleep_for(ms);
		tw.AdvanceClock(i);
	}

    tw.Shutdown();

	delete t1;
	delete t5;
	delete t10;
	delete t16;
	delete t160;
	return 0;
}
```

# 注意事项
* 专为游戏服务器设计，未过多考虑通用性
* 非线程安全
* 时间单位: 毫秒
* 为保证任务的添加和处理不同步执行，加入已经到期的定时任务时，任务不会立即被派发处理，而是在下次推进时派发处理

## License

gamewheels-timingwheel source code is available under the MIT [License](/LICENSE).

