#pragma once

#ifndef TW_MALLOC
#define TW_MALLOC(_SIZE_) new char[_SIZE_]
#endif
#ifndef TW_FREE
#define TW_FREE(_PTR_) delete _PTR_
#endif
#ifndef TW_FREE_ARRAY
#define TW_FREE_ARRAY(_PTR_) delete[] _PTR_
#endif

template <typename Task>
class TTimerTaskList
{
protected:
    struct TimerTaskEntry
    {
        TTimerTaskList<Task> *list;
        TimerTaskEntry *prev;
        TimerTaskEntry *next;
        Task *timerTask;
        long expiration;

        TimerTaskEntry(Task *timerTask, long expiration)
            : list(nullptr), prev(nullptr), next(nullptr),
              timerTask(timerTask), expiration(expiration)
        {
            if (timerTask != nullptr)
            {
                timerTask->SetTimerTaskEntry(this);
            }
        }
    };

public:
    typedef void (*TimerTaskHandler)(Task *);
    typedef void (*TimerTaskTarget)(void *, Task *);
    TTimerTaskList()
    {
        root_ = new (TW_MALLOC(sizeof(TimerTaskEntry)))
            TimerTaskEntry(nullptr, -1);
        tail_ = root_;
        counter_ = 0;
    }

    ~TTimerTaskList()
    {
        Clear();
        TW_FREE(root_);
    }

    void Add(Task *timerTask)
    {
        auto *entry = new (TW_MALLOC(sizeof(TimerTaskEntry)))
            TimerTaskEntry(timerTask, timerTask->GetExpiration());
        entry->prev = tail_;
        entry->list = this;
        tail_->next = entry;
        tail_ = entry;
        tail_->list = this;
        ++counter_;
    }

    bool Remove(Task *timerTask)
    {
        auto *entry = static_cast<TimerTaskEntry *>(timerTask->GetTimerTaskEntry());
        if (entry != nullptr)
        {
            if (entry->list == this)
            {
                entry->prev->next = entry->next;
                --counter_;
                timerTask->SetTimerTaskEntry(nullptr);
                TW_FREE(entry);
                return true;
            }
        }
        return false;
    }

    long GetCount()
    {
        return counter_;
    }

    void ForEach(TimerTaskHandler handler)
    {
        auto *entry = root_->next;
        while (entry != nullptr)
        {
            handler(entry->timerTask);
            entry = entry->next;
        }
    }

    void ForEach(TimerTaskTarget target, void *obj)
    {
        auto *entry = root_->next;
        while (entry != nullptr)
        {
            target(obj, entry->timerTask);
            entry = entry->next;
        }
    }

    void Clear()
    {
        auto entry = root_->next;
        while (entry != nullptr)
        {
            entry->timerTask->SetTimerTaskEntry(nullptr);
            auto next = entry->next;
            TW_FREE(entry);
            entry = next;
        }
        root_->next = nullptr;
        tail_ = root_;
        counter_ = 0;
    }

private:
    TimerTaskEntry *root_;
    TimerTaskEntry *tail_;
    long counter_;
};

template <typename Task>
class TTimingWheel
{
public:
    typedef typename TTimerTaskList<Task>::TimerTaskHandler TimerTaskHandler;
    typedef typename TTimerTaskList<Task>::TimerTaskTarget TimerTaskTarget;

    TTimingWheel(int tickMs, int wheelSize, long startMs, TimerTaskHandler taskHandler)
        : TTimingWheel(tickMs, wheelSize, startMs, taskHandler, nullptr)
    {
    }

private:
    TTimingWheel(int tickMs, int wheelSize, long startMs,
                 TimerTaskHandler taskHandler, TTimingWheel<Task> *lowerWheel)
        : tickMs_(tickMs), wheelSize_(wheelSize),
          taskCounter_(0), overflowWheel_(nullptr),
          lowerWheel_(lowerWheel), taskHandler_(taskHandler)
    {
        currentTime_ = startMs - (startMs % tickMs);
        interval_ = tickMs * wheelSize;
        buckets_ = new (TW_MALLOC(sizeof(TTimerTaskList<Task> *) * wheelSize_))
            TTimerTaskList<Task> *[wheelSize];
        for (int i = 0; i < wheelSize; ++i)
        {
            buckets_[i] = new (TW_MALLOC(sizeof(TTimerTaskList<Task>)))
                TTimerTaskList<Task>();
        }
    }

public:
    ~TTimingWheel()
    {
        for (int i = 0; i < wheelSize_; ++i)
        {
            TW_FREE(buckets_[i]);
        }
        TW_FREE_ARRAY(buckets_);
        if (overflowWheel_ != nullptr)
        {
            TW_FREE(overflowWheel_);
        }
    }

private:
    static void AddTo(void *that, Task *timerTask)
    {
        static_cast<TTimingWheel<Task> *>(that)->Add(timerTask);
    }

public:
    void AdvanceClock(long timeMs)
    {
        while (timeMs >= currentTime_)
        {
            if (overflowWheel_ != nullptr)
            {
                overflowWheel_->AdvanceClock(currentTime_);
            }
            auto *bucket = buckets_[(currentTime_ % interval_) / tickMs_];
            if (bucket->GetCount() > 0)
            {
                if (lowerWheel_ == nullptr)
                {
                    bucket->ForEach(taskHandler_);
                }
                else
                {
                    bucket->ForEach(AddTo, lowerWheel_);
                }
                taskCounter_ -= bucket->GetCount();
                bucket->Clear();
            }
            currentTime_ += tickMs_;
        }
    }

    long GetCount()
    {
        long counter = taskCounter_;
        if (overflowWheel_ != nullptr)
        {
            counter += overflowWheel_->GetCount();
        }
        return counter;
    }

    void Add(Task *timerTask)
    {
        long expiration = timerTask->GetExpiration();
        if (expiration < currentTime_ + interval_)
        {
            TTimerTaskList<Task> *bucket;
            if (expiration < currentTime_)
            {
                bucket = buckets_[(currentTime_ % interval_) / tickMs_];
            }
            else
            {
                bucket = buckets_[(expiration % interval_) / tickMs_];
            }
            bucket->Add(timerTask);
            ++taskCounter_;
        }
        else
        {
            if (overflowWheel_ == nullptr)
            {
                overflowWheel_ = new (TW_MALLOC(sizeof(TTimingWheel)))
                    TTimingWheel(interval_, wheelSize_, currentTime_, nullptr, this);
            }
            overflowWheel_->Add(timerTask);
        }
    }

    bool Remove(Task *timerTask)
    {
        long expiration = timerTask->GetExpiration();
        if (expiration < currentTime_ + interval_)
        {
            auto bucket = buckets_[expiration / tickMs_];
            if (bucket->Remove(timerTask))
            {
                --taskCounter_;
                return true;
            }
        }
        else
        {
            if (overflowWheel_ != nullptr)
            {
                return overflowWheel_->Remove(timerTask);
            }
        }
        return false;
    }

    void Shutdown()
    {
        if (overflowWheel_ != nullptr)
        {
            overflowWheel_->Shutdown();
        }
        for (int i = 0; i < wheelSize_; ++i)
        {
            buckets_[i]->Clear();
        }
    }

private:
    int tickMs_;
    int wheelSize_;
    long taskCounter_;
    long currentTime_;
    long interval_;
    
    TTimingWheel<Task> *overflowWheel_;
    TTimingWheel<Task> *lowerWheel_;
    TTimerTaskList<Task> **buckets_;
    TimerTaskHandler taskHandler_;
};