// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "inMath.hpp"
#include <cmath>

Lut1D<256> fiveSineExpIsh(0.f, 1.f);  // 5-nested bSin convex curve: slow start, fast end (WaveShaper2, LFO1, …)
Lut1D<256> fiveSineLogIsh(0.f, 1.f);  // 5-nested bSin concave curve: fast start, slow end (same modules)
Lut1D<256> normExp(0.f, 1.f);         // Normalized exp'ish ramp (convex): slow start, fast end
Lut1D<256> normLog(0.f, 1.f);         // Normalized log'ish ramp (concave): fast start, slow end
Lut1D<256> depthPow2Lut(1.f, 16.f); // Used by RateDephtReducer (2^1-16 bits)

static bool s_fiveSineExpLogLutsInitialized = false;
static bool s_normExpLogLutsInitialized = false;
static bool s_depthPow2LutInitialized = false;

// Steepness for normExp / normLog; higher = more curved, still 0 at 0 and 1 at 1.
static const float kNormExpLog = 4.f;

/// @brief Ensures fiveSineExpIsh / fiveSineLogIsh are initialized.
/// Must be called before using those lookup tables. Idempotent.
void ensureFiveSineExpLogLuts() {
    if (s_fiveSineExpLogLutsInitialized)
        return;

    const int n = fiveSineExpIsh.Size;
    for (int i = 1; i < n - 1; i++) { // Skip first and last values
        float x = (float)i / (n - 1);
        fiveSineExpIsh.data[i] = (bSin((bSin((bSin((bSin((bSin(x * 0.25f + 0.75f) + 1.f) * 0.25f + 0.75f) + 1.f) * 0.25f + 0.75f) + 1.f) * 0.25f + 0.75f) + 1.f) * 0.25f + 0.75f) + 1.f);
        fiveSineLogIsh.data[i] = bSin(bSin(bSin(bSin(bSin(x * 0.25f) * 0.25f) * 0.25f) * 0.25f) * 0.25f);
    }

    // Ensure the first and last values are exactly 0 and 1
    fiveSineExpIsh.data[0] = 0.f;
    fiveSineExpIsh.data[n - 1] = 1.f;
    fiveSineLogIsh.data[0] = 0.f;
    fiveSineLogIsh.data[n - 1] = 1.f;

    s_fiveSineExpLogLutsInitialized = true;
}

/// @brief Ensures normExp / normLog are initialized.
/// Must be called before using those lookup tables. Idempotent.
void ensureNormExpLogLuts() {
    if (s_normExpLogLutsInitialized)
        return;

    const int n = normExp.Size;
    const float expK = std::exp(kNormExpLog);
    const float expNegK = std::exp(-kNormExpLog);
    const float concaveDenom = 1.f - expNegK; // for (1 - exp(-k*x)): log'ish, fast start, slow end
    const float convexDenom = expK - 1.f;     // for (exp(k*x) - 1):  exp'ish, slow start, fast end

    for (int i = 1; i < n - 1; i++) { // Skip first and last values
        float x = (float)i / (n - 1);
        normLog.data[i] = (1.f - std::exp(-kNormExpLog * x)) / concaveDenom;
        normExp.data[i] = (std::exp(kNormExpLog * x) - 1.f) / convexDenom;
    }

    normExp.data[0] = 0.f;
    normExp.data[n - 1] = 1.f;
    normLog.data[0] = 0.f;
    normLog.data[n - 1] = 1.f;

    s_normExpLogLutsInitialized = true;
}

/// @brief Ensures the depth power of 2 lookup table is initialized.
/// Must be called by modules useing depthPow2Lut before using the lookup tables.
void ensureDepthPow2Lut() {
    if (s_depthPow2LutInitialized)
        return;

    const int n = depthPow2Lut.Size;
    const float minBits = depthPow2Lut.minX;
    const float maxBits = depthPow2Lut.maxX;
    const float span = maxBits - minBits;
    for (int i = 0; i < n; i++) {
        float t = (float)i / (n - 1);
        float bits = minBits + span * t;   // 1..16
        depthPow2Lut.data[i] = std::pow(2.f, bits);
    }
    s_depthPow2LutInitialized = true;
}
