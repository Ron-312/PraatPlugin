#include "audio/WavFileWriter.h"

WavFileWriter::WriteResult WavFileWriter::writeAudioBufferToWavFile (
    const juce::AudioBuffer<float>& buffer,
    double sampleRate,
    const juce::File& destinationFile)
{
    if (buffer.getNumSamples() == 0)
        return WriteResult::NothingToWrite;

    // Ensure the parent directory exists.
    destinationFile.getParentDirectory().createDirectory();

    juce::WavAudioFormat wavFormat;

    std::unique_ptr<juce::AudioFormatWriter> writer (
        wavFormat.createWriterFor (
            new juce::FileOutputStream (destinationFile),
            sampleRate,
            static_cast<unsigned int> (buffer.getNumChannels()),
            24,    // 24-bit WAV — good quality, widely supported
            {},
            0));

    if (writer == nullptr)
        return WriteResult::DestinationFileCouldNotBeCreated;

    const bool writeSucceeded = writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());

    return writeSucceeded ? WriteResult::Success : WriteResult::WriteError;
}

juce::String WavFileWriter::describeWriteResult (WriteResult result)
{
    switch (result)
    {
        case WriteResult::Success:                            return "Audio written successfully";
        case WriteResult::DestinationFileCouldNotBeCreated:  return "Could not create output file";
        case WriteResult::NothingToWrite:                     return "No audio captured";
        case WriteResult::WriteError:                         return "Error writing audio data";
    }
    return "Unknown error";
}
