#include "audio/AudioCapture.h"
#include "audio/WavFileWriter.h"

AudioCapture::AudioCapture()
    : fifoController_ (1),   // minimal initial size; resized in prepareForCapture
      ringBuffer_ (1, 1)
{
}

void AudioCapture::prepareForCapture (double sampleRate, int numberOfChannels)
{
    currentSampleRate_        = sampleRate;
    currentNumberOfChannels_  = numberOfChannels;
    totalSamplesWritten_      = 0;

    const int capacityInSamples = static_cast<int> (
        maximumCaptureDurationInSeconds * sampleRate);

    // Resize the ring buffer to hold the maximum capture window.
    // This allocation happens on the message thread during prepareToPlay,
    // never during processBlock. See ADR-003.
    ringBuffer_.setSize (numberOfChannels, capacityInSamples);
    fifoController_.setTotalSize (capacityInSamples);
}

void AudioCapture::appendSamplesFromProcessBlock (const juce::AudioBuffer<float>& block)
{
    // AUDIO-THREAD SAFE: lock-free write via AbstractFifo. No allocation, no mutex.
    const int numSamplesToWrite = block.getNumSamples();

    int startIndex1, blockSize1, startIndex2, blockSize2;
    fifoController_.prepareToWrite (numSamplesToWrite,
                                    startIndex1, blockSize1,
                                    startIndex2, blockSize2);

    for (int channel = 0; channel < ringBuffer_.getNumChannels(); ++channel)
    {
        const int srcChannel = juce::jmin (channel, block.getNumChannels() - 1);

        if (blockSize1 > 0)
            ringBuffer_.copyFrom (channel, startIndex1,
                                  block, srcChannel, 0, blockSize1);
        if (blockSize2 > 0)
            ringBuffer_.copyFrom (channel, startIndex2,
                                  block, srcChannel, blockSize1, blockSize2);
    }

    fifoController_.finishedWrite (blockSize1 + blockSize2);
    totalSamplesWritten_ += (blockSize1 + blockSize2);

    // Track peak amplitude for the editor's VU meter (atomic, no locking).
    float peak = 0.0f;
    for (int ch = 0; ch < block.getNumChannels(); ++ch)
        peak = juce::jmax (peak, block.getMagnitude (ch, 0, numSamplesToWrite));
    latestPeakLevel_.store (peak, std::memory_order_relaxed);
}

bool AudioCapture::hasAudioReadyForAnalysis() const noexcept
{
    return totalSamplesWritten_ > 0;
}

int AudioCapture::numberOfSamplesAvailable() const noexcept
{
    return fifoController_.getNumReady();
}

AudioCapture::WriteResult AudioCapture::writeCapturedAudioToWavFile (
    const juce::File& destinationFile) const
{
    // Called on the worker thread only. Never on the audio thread.
    const int numSamplesAvailable = fifoController_.getNumReady();

    if (numSamplesAvailable == 0)
        return WriteResult::NoAudioCaptured;

    // Copy the ring buffer contents into a linear AudioBuffer for writing.
    juce::AudioBuffer<float> linearBuffer (ringBuffer_.getNumChannels(), numSamplesAvailable);

    int startIndex1, blockSize1, startIndex2, blockSize2;
    const_cast<juce::AbstractFifo&> (fifoController_).prepareToRead (
        numSamplesAvailable, startIndex1, blockSize1, startIndex2, blockSize2);

    for (int channel = 0; channel < ringBuffer_.getNumChannels(); ++channel)
    {
        if (blockSize1 > 0)
            linearBuffer.copyFrom (channel, 0,
                                   ringBuffer_, channel, startIndex1, blockSize1);
        if (blockSize2 > 0)
            linearBuffer.copyFrom (channel, blockSize1,
                                   ringBuffer_, channel, startIndex2, blockSize2);
    }

    const_cast<juce::AbstractFifo&> (fifoController_).finishedRead (blockSize1 + blockSize2);

    const auto writeResult = WavFileWriter::writeAudioBufferToWavFile (
        linearBuffer, currentSampleRate_, destinationFile);

    if (writeResult != WavFileWriter::WriteResult::Success)
        return WriteResult::FileWriteFailed;

    return WriteResult::Success;
}

float AudioCapture::latestPeakLevel() const noexcept
{
    return latestPeakLevel_.load (std::memory_order_relaxed);
}

void AudioCapture::copyCurrentContentsTo (juce::AudioBuffer<float>& dest) const
{
    // Non-consuming peek: compute read indices but do NOT advance the read pointer.
    // Safe because only the message thread calls prepareToRead / finishedRead,
    // and the audio thread only calls prepareToWrite / finishedWrite.
    const int numReady = fifoController_.getNumReady();

    if (numReady == 0)
    {
        dest.setSize (currentNumberOfChannels_, 0, false, true, false);
        return;
    }

    dest.setSize (ringBuffer_.getNumChannels(), numReady, false, true, false);

    int startIndex1, blockSize1, startIndex2, blockSize2;
    const_cast<juce::AbstractFifo&> (fifoController_).prepareToRead (
        numReady, startIndex1, blockSize1, startIndex2, blockSize2);

    for (int ch = 0; ch < ringBuffer_.getNumChannels(); ++ch)
    {
        if (blockSize1 > 0)
            dest.copyFrom (ch, 0,         ringBuffer_, ch, startIndex1, blockSize1);
        if (blockSize2 > 0)
            dest.copyFrom (ch, blockSize1, ringBuffer_, ch, startIndex2, blockSize2);
    }

    // Intentionally do NOT call finishedRead — this is a display-only snapshot.
}

void AudioCapture::clearCaptureBuffer()
{
    // Drain the fifo by reading everything available.
    const int available = fifoController_.getNumReady();
    if (available > 0)
    {
        int s1, b1, s2, b2;
        fifoController_.prepareToRead (available, s1, b1, s2, b2);
        fifoController_.finishedRead (b1 + b2);
    }
    totalSamplesWritten_ = 0;
}
