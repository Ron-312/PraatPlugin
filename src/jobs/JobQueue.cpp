#include "jobs/JobQueue.h"

void JobQueue::enqueueAnalysisJob (AnalysisJob job)
{
    {
        std::lock_guard<std::mutex> lock (queueMutex_);
        pendingJobs_.push (std::move (job));
    }
    jobAvailableSignal_.notify_one();
}

bool JobQueue::waitForNextJobAndDequeue (AnalysisJob& outJob)
{
    std::unique_lock<std::mutex> lock (queueMutex_);

    jobAvailableSignal_.wait (lock, [this]
    {
        return ! pendingJobs_.empty() || shouldStop_;
    });

    if (shouldStop_ && pendingJobs_.empty())
        return false;

    outJob = std::move (pendingJobs_.front());
    pendingJobs_.pop();
    return true;
}

void JobQueue::signalConsumerToStop()
{
    {
        std::lock_guard<std::mutex> lock (queueMutex_);
        shouldStop_ = true;
    }
    jobAvailableSignal_.notify_all();
}

bool JobQueue::hasJobsWaiting() const noexcept
{
    std::lock_guard<std::mutex> lock (queueMutex_);
    return ! pendingJobs_.empty();
}

int JobQueue::numberOfJobsWaiting() const noexcept
{
    std::lock_guard<std::mutex> lock (queueMutex_);
    return static_cast<int> (pendingJobs_.size());
}
