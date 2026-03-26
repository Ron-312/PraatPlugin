#pragma once

#include <juce_core/juce_core.h>
#include "jobs/AnalysisJob.h"
#include <queue>
#include <mutex>
#include <condition_variable>

// A thread-safe FIFO of AnalysisJobs.
//
// Producer: message thread (PraatPluginProcessor::beginAnalysisOfCapturedAudio)
// Consumer: JobDispatcher worker thread
//
// Jobs are enqueued with state Pending and dequeued for execution.
// The queue does not execute jobs — it only stores and hands them off.
class JobQueue
{
public:
    JobQueue()  = default;
    ~JobQueue() = default;

    // Adds a job to the back of the queue.
    // Thread-safe. Wakes the consumer if it is waiting.
    void enqueueAnalysisJob (AnalysisJob job);

    // Removes the front job and writes it to outJob.
    // Blocks until a job is available OR until shouldStop() returns true.
    // Returns false (without writing to outJob) if the queue was stopped.
    bool waitForNextJobAndDequeue (AnalysisJob& outJob);

    // Signals the consumer to stop waiting and return false from waitForNextJobAndDequeue.
    // Call this before destroying JobDispatcher to unblock the worker thread.
    void signalConsumerToStop();

    bool hasJobsWaiting() const noexcept;
    int  numberOfJobsWaiting() const noexcept;

private:
    mutable std::mutex          queueMutex_;
    std::queue<AnalysisJob>     pendingJobs_;
    std::condition_variable     jobAvailableSignal_;
    bool                        shouldStop_ { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JobQueue)
};
