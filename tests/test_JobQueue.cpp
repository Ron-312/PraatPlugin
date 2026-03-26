#include <catch2/catch_test_macros.hpp>
#include "jobs/JobQueue.h"
#include <thread>

TEST_CASE ("JobQueue enqueue and dequeue single job", "[JobQueue]")
{
    JobQueue queue;

    AnalysisJob job;
    job.jobIdentifier  = juce::Uuid();
    job.currentState   = JobState::Pending;
    job.praatScriptFile = juce::File ("/tmp/test_script.praat");

    queue.enqueueAnalysisJob (job);

    REQUIRE (queue.hasJobsWaiting());
    REQUIRE (queue.numberOfJobsWaiting() == 1);

    AnalysisJob dequeued;
    const bool gotJob = queue.waitForNextJobAndDequeue (dequeued);

    REQUIRE (gotJob);
    REQUIRE (! queue.hasJobsWaiting());
    REQUIRE (dequeued.praatScriptFile == job.praatScriptFile);
}

TEST_CASE ("JobQueue FIFO ordering", "[JobQueue]")
{
    JobQueue queue;

    for (int i = 0; i < 3; ++i)
    {
        AnalysisJob job;
        job.errorDescription = juce::String (i);
        queue.enqueueAnalysisJob (job);
    }

    REQUIRE (queue.numberOfJobsWaiting() == 3);

    for (int expectedOrder = 0; expectedOrder < 3; ++expectedOrder)
    {
        AnalysisJob dequeued;
        queue.waitForNextJobAndDequeue (dequeued);
        REQUIRE (dequeued.errorDescription == juce::String (expectedOrder));
    }
}

TEST_CASE ("JobQueue signalConsumerToStop unblocks waiting thread", "[JobQueue]")
{
    JobQueue queue;

    bool threadReturnedFalse = false;

    std::thread consumer ([&]
    {
        AnalysisJob job;
        const bool gotJob = queue.waitForNextJobAndDequeue (job);
        threadReturnedFalse = ! gotJob;
    });

    // Give the consumer time to start waiting.
    std::this_thread::sleep_for (std::chrono::milliseconds (50));

    queue.signalConsumerToStop();
    consumer.join();

    REQUIRE (threadReturnedFalse);
}
