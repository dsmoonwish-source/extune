#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
// YIN pitch detector (monophonic). Push samples, call detect() every hop.
// ---------------------------------------------------------------------------
struct YinDetector
{
    static constexpr int frameSize = 2048;
    static constexpr int window    = 1024;

    void push (float x)
    {
        ring[(size_t) pos] = x;
        pos = (pos + 1) % frameSize;
    }

    // Returns frequency in Hz, or 0 if no clear pitch.
    float detect (double sr, float threshold = 0.15f)
    {
        float energy = 0.f;
        for (int i = 0; i < frameSize; ++i)
        {
            frame[(size_t) i] = ring[(size_t) ((pos + i) % frameSize)];
            energy += frame[(size_t) i] * frame[(size_t) i];
        }
        if (std::sqrt (energy / frameSize) < 0.003f) // silence gate
            return 0.f;

        const int tauMax = std::min (window, (int) (sr / 70.0));   // lowest ~70 Hz
        const int tauMin = std::max (2, (int) (sr / 1000.0));      // highest ~1 kHz

        float running = 0.f;
        d[0] = 1.f;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            float sum = 0.f;
            for (int j = 0; j < window; ++j)
            {
                const float diff = frame[(size_t) j] - frame[(size_t) (j + tau)];
                sum += diff * diff;
            }
            running += sum;
            d[(size_t) tau] = running > 0.f ? sum * (float) tau / running : 1.f;
        }

        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (d[(size_t) tau] < threshold)
            {
                while (tau + 1 < tauMax && d[(size_t) tau + 1] < d[(size_t) tau])
                    ++tau;

                const float x0 = d[(size_t) tau - 1], x1 = d[(size_t) tau], x2 = d[(size_t) tau + 1];
                const float denom = x0 + x2 - 2.f * x1;
                const float shift = std::abs (denom) > 1e-9f ? (x0 - x2) / (2.f * denom) : 0.f;
                return (float) (sr / ((double) tau + shift));
            }
        }
        return 0.f;
    }

private:
    std::vector<float> ring  = std::vector<float> (frameSize, 0.f);
    std::vector<float> frame = std::vector<float> (frameSize, 0.f);
    std::vector<float> d     = std::vector<float> (window + 1, 1.f);
    int pos = 0;
};

// ---------------------------------------------------------------------------
// Two-tap crossfaded delay-line pitch shifter.
// ratio > 1 shifts up, ratio < 1 shifts down.
// ---------------------------------------------------------------------------
struct PitchShifter
{
    static constexpr int size = 16384;
    static constexpr int mask = size - 1;

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.f);
        w = 0;
        phase = 0.f;
    }

    float process (float in, float ratio, float grain)
    {
        buf[(size_t) w] = in;

        phase += (1.f - ratio) / grain;
        phase -= std::floor (phase);

        float out = 0.f;
        for (int i = 0; i < 2; ++i)
        {
            float p = phase + 0.5f * (float) i;
            if (p >= 1.f) p -= 1.f;

            const float delay = p * grain + 1.f;
            const float rp    = (float) w - delay;
            const int   i0    = (int) std::floor (rp);
            const float fr    = rp - (float) i0;

            const float a = buf[(size_t) (i0 & mask)];
            const float b = buf[(size_t) ((i0 + 1) & mask)];
            const float win = 0.5f - 0.5f * std::cos (6.2831853f * p);

            out += (a + fr * (b - a)) * win;
        }

        w = (w + 1) & mask;
        return out;
    }

private:
    std::vector<float> buf = std::vector<float> (size, 0.f);
    int w = 0;
    float phase = 0.f;
};
