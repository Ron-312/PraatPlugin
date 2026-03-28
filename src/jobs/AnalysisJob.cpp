#include "jobs/AnalysisJob.h"

juce::String describeJobState (JobState state)
{
    switch (state)
    {
        case JobState::Pending:               return "Pending";
        case JobState::Running:               return "Running";
        case JobState::CompletedSuccessfully: return "Completed";
        case JobState::FailedWithError:       return "Failed";
        case JobState::Cancelled:             return "Cancelled";
    }
    return "Unknown";
}
