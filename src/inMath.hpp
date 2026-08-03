// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#pragma once
#include <rack.hpp>
//using namespace ::rack; // if enabled, prefixed "rack::" is not necessary
//#include <math.h>

//#define INM_PI		3.14159265358979323846 // Apparently M_PI can be an issus building for MacOS

//-----------------------------------------------------------------------------
// Random related functions
//-----------------------------------------------------------------------------

///  @brief MWC (multiply with carry) Random number generator, 
///  based on George Marsaglia's MWC generator.
struct RandMWC {
    uint32_t _z;
    uint32_t _w;
    
    RandMWC(uint32_t seed) {
		_z = 362436069;
		_w = 521288629 ^ seed;
	}

    inline void seed(uint32_t seed) {
		_z = 362436069;
		_w = 521288629 ^ seed;
	}

    inline uint32_t next() {
        _z = 36969 * (_z & 65535) + (_z >> 16);
        _w = 18000 * (_w & 65535) + (_w >> 16);
        return (_z << 16) + _w;
    }

    inline float nextFloat() {
		// Divide by 2^32 (not 0xFFFFFFFF): float cannot represent 2^32-1 exactly
		return (float)next() / 4294967296.f;
	}
};

/// @brief Generate a random number (equal chance) between 0
/// and UINT32_MAX.
/// @return Random number between 0 and UINT32_MAX (eaqual chance).
inline uint32_t  RandomUint32() {
    return rack::random::get<uint32_t >();
}

/// @brief Generate a random number (equal chance) between 0 (inlc.) 
/// and 1 (excl.).
/// @return Random number between 0 and 1 (eaqual chance).
inline float randomNorm() {
  return rack::random::get<float>();
}

/// @brief Generate a random number between 0 (incl.) and 1 (excl.) with 
/// a triangular distribution.
/// @return Random number between 0 and 1 with a triangular distribution.
inline float randomTriangular() {
  return (rack::random::get<float>() + rack::random::get<float>()) / 2.f;
}

/// @brief Generate a random number between 0 (incl.) and 1 (excl.), with smaller
/// chance for 0, and larger chance for 1 (based on triangular distribution).
inline float randomTriInc() {
    float triRnd = 1.f - fabs((rack::random::get<float>() + rack::random::get<float>()) - 1.f);
    return triRnd >= 0.99999f ? 0.99999 : triRnd;  // Avoid 1.0
}

/// @brief Generate a random number between 0 (incl.) and 1 (excl.), with larger
/// chance for 0, and smaller chance for 1 (based on triangular distribution).
inline float randomTriDec() {
    return fabs((rack::random::get<float>() + rack::random::get<float>()) - 1.f);
}

/// @brief Generate a random number between 0 (incl.) and 1 (excl.) with 
/// a Gaussian'ish distribution.
/// @return Random number between 0 and 1 with a Gaussian'ish distribution.
inline float randomGaussian() {
  return (rack::random::get<float>() + rack::random::get<float>() + rack::random::get<float>()) / 3.f;
}

/// @brief Generate a random number between 0 (incl.) and 1 (excl.), with smaller
/// chance for 0, and larger chance for 1 (based on Gaussian'ish distribution).
inline float randomGausInc() {
    float triRnd = 1.f - fabs((rack::random::get<float>() + rack::random::get<float>() + rack::random::get<float>()) - 1.5f) / 1.5f;
    return triRnd >= 0.99999f ? 0.99999 : triRnd;  // Avoid 1.0
}

/// @brief Generate a random number between 0 (incl.) and 1 (excl.), with larger
/// chance for 0, and smaller chance for 1 (based on Gaussian'ish distribution).
inline float randomGausDec() {
    return fabs((rack::random::get<float>() + rack::random::get<float>() + rack::random::get<float>()) - 1.5f) / 1.5f;
}

/// @brief Random multiplicative factor for per-cycle "rate chaos" on a phase-step.
/// @param chaos Chaos amount 0..1. 0 -> 1.0 (off). 1.0 -> factor in [0.1, 10],
/// uniform-in-log. Low values stay near 1.0 (triangular), fading to uniform as
/// chaos rises. Symmetric in log, so factor and 1/factor are equally likely.
inline float rateChaosFactor(float chaos) {
    if (chaos <= 0.f)
        return 1.f;
    float u1 = randomNorm();
    float u2 = randomNorm();
    float x = u1 * (1.f + chaos) + u2 * (1.f - chaos) - 1.f; // [-1,1], mean 0
    // 10^(chaos*x) == 2^(chaos*x*log2(10)); log2(10) approx 3.321928f
    return rack::dsp::exp2_taylor5(chaos * x * 3.321928f);
}

/// @brief Generate a random number in the specified range (between minValue 
/// and maxValue) with the specified distribution (and distribution mode).
/// @param minValue The minimum value of the range (can be greater than maxValue).
/// @param maxValue The maximum value of the range (can be less than minValue).
/// @param dist The distribution of the random number (clamped between -1 and 1).
/// @param minMaxDistMode If true, the distribution-mode will be Min/Max,
/// otherwise it will be Center/Edge.
/// @param forcedPolarity If true, the sign of the random number will be forced
/// to toggle bewtween the top-/lower half between min and max (based on prevSign).
/// @param prevSign The previous sign (-1 or +1) of the previous random number (used for forcedPolarity,
/// and will be updated/changed during the call if applicable).
inline float randomMinMaxDist(float minValue, float maxValue, float dist, bool minMaxDistMode,
    bool forcedPolarity, float &prevSign) {
    if (minValue > maxValue) {
		float temp = minValue;
		minValue = maxValue;
		maxValue = temp;
	}
    float range = maxValue - minValue;
    if (range == 0.f)
        return minValue;

    dist = rack::math::clamp(dist, -1.f, 1.f);
    float aDist = fabs(dist);

    // generate rndRange according to aDist
    float nonUniRange = 1.f - aDist;
    float uniRange = (aDist >= 0.6)
        ? nonUniRange * 1.f / 0.4f
        : 1.f;
    float uniChance = randomNorm() * (1.f - aDist);
    float rndRange = uniChance >= randomNorm()
        ? uniRange * randomNorm()
        : nonUniRange * randomNorm();

    //--- Min/Max-distribution --------------------------------------------
    if (minMaxDistMode) {
        return (dist < 0.f)
            ? minValue + rndRange * range
            : maxValue - rndRange * range;
    }

    //--- Center/Edge-distribution ----------------------------------------
    float sign = (forcedPolarity)
        ? prevSign * -1.f // Forced polarity (toggle sign)
        : randomNorm() < 0.5f ? -1.f : 1.f; // Random sign
    prevSign = sign;
    float halfRange = range / 2.f;
    float midValue = minValue + halfRange;
    return (dist < 0.f)
        ? midValue - rndRange * halfRange * sign
        : sign < 0.f
            ? minValue + rndRange * halfRange
            : maxValue - rndRange * halfRange;
}

//-----------------------------------------------------------------------------
// Misc
//-----------------------------------------------------------------------------

/// @brief Fast Sin "calculation", via Bhaskara I's sine approximation formula.
/// @param phase (0 to 1). Simply add 0.25 to have a cosine result.
/// @return Approximation of Sin(p*2*pi), in range -1 to 1.
inline float bSin(float p) {
    p -= std::floorf(p);
    float t;
    // 0.3125 = 5/16
    if (p < 0.5f) {
        t = p * (0.5f - p);
        return 4.f * t / (0.3125f - t);
    }

    t = (p - 0.5f) * (p - 1.f);
    return 4.f * t / (0.3125f + t);
}

/// @brief Normalized Fast Sin "calculation", via Bhaskara I's sine approximation formula.
/// @param phase (0 to 1). Simply add 0.25 to have a cosine result.
/// @return Approximation of Sin(p*2*pi), in range 0 to 1.
inline float bSinNorm(float p) {
    p -= std::floorf(p);
    float t;
    // 0.3125 = 5/16
    if (p < 0.5f) {
        t = p * (0.5f - p);
        return (4.f * t / (0.3125f - t) + 1.f) / 2.f;  // Normalize to 0-1
    }

    t = (p - 0.5f) * (p - 1.f);
    return (4.f * t / (0.3125f + t) + 1.f) / 2.f;  // Normalize to 0-1
}

//-----------------------------------------------------------------------------
// Lookup tables
//-----------------------------------------------------------------------------

/// @brief 1D lookup table with linear interpolation.
/// @tparam N Number of table entries.
template<int N>
struct Lut1D {
    static constexpr int Size = N;
    float data[N] = {};
    float minX = 0.f;
    float maxX = 1.f;

    Lut1D() = default;
    Lut1D(float minVal, float maxVal) {
        minX = (minVal <= maxVal) ? minVal : maxVal;
        maxX = (minVal >= maxVal) ? minVal : maxVal;
        if (maxX <= minX)
            maxX = minX + 1e-6f;
    }

    /// @brief Get interpolated value. x in [minX, maxX], clamped if out of range.
    inline float lookup(float x) const {
        x = rack::math::clamp(x, minX, maxX);
        float t = (x - minX) / (maxX - minX);
        float fidx = t * (N - 1);
        int lo = (int)fidx;
        if (lo >= N - 1)
            return data[N - 1];
        float frac = fidx - lo;
        return data[lo] * (1.f - frac) + data[lo + 1] * frac;
    }

    inline float operator()(float x) const { return lookup(x); }
};

/// @brief 5-nested bSin LUTs: fiveSineExpIsh = convex (slow start), fiveSineLogIsh = concave (fast start).
extern Lut1D<256> fiveSineExpIsh;
extern Lut1D<256> fiveSineLogIsh;

/// @brief Normalized exp/log ramp LUTs on [0, 1] (fast-then-slow vs slow-then-fast).
extern Lut1D<256> normExp;
extern Lut1D<256> normLog;

/// @brief Ensures fiveSineExpIsh / fiveSineLogIsh are filled. Call before first use. Idempotent.
void ensureFiveSineExpLogLuts();

/// @brief Ensures normExp / normLog are filled. Call before first use. Idempotent.
void ensureNormExpLogLuts();

/// @brief LUT for mapping continuous bit-depth 1..16 to 2^bits (2..65536 levels).
extern Lut1D<256> depthPow2Lut;

/// @brief Ensures depthPow2Lut is filled. Call before first use. Idempotent.
void ensureDepthPow2Lut();

//-----------------------------------------------------------------------------
// Misc (continued)
//-----------------------------------------------------------------------------

/// @brief Generates an S-curve (two quarter sine waves) between 0 and 1.
/// @param phase Phase (clamped 0 to 1), determines position on the S-curve.
/// @return The value on the S-curve, in range [0, 1].
inline float sCurve(float phase) {
    phase = rack::math::clamp(phase, 0.f, 1.f);

    if (phase < 0.5f)
    	return (bSin(phase * 0.5f + 0.75f) + 1.f) * 0.5f;

    phase -= 0.5f;
    return 0.5f + bSin(phase * 0.5f) * 0.5f;
}

/// @brief Generates a reversed S-curve (two quarter sine waves) between 0 and 1.
/// @param phase Phase (clamped 0 to 1), determines position on the S-curve.
/// @return The value on the S-curve, in range [0, 1].
inline float sCurveRev(float phase) {
    phase = rack::math::clamp(phase, 0.f, 1.f);

    if (phase < 0.5f)
        return bSin(phase * 0.5f) * 0.5f;

    phase -= 0.5f;
    return 0.5f + (bSin(phase * 0.5f + 0.75f) + 1.f) * 0.5f;
}

// https://github.com/hires/Dintree-Virtual/blob/master/src/dsp_utils.h
// https://github.com/hires/Dintree-Virtual/blob/master/src/V107-Dual_Slew.cpp

/// @brief Calculates one-pole filter-coefficient for a given frequency and audio sample rate.
/// @param frequency The cutoff frequency of the filter.
/// @param sampleRate The audio sample rate.
/// @return The one-pole filter-coefficient.
inline float calcOnePoleCoeff(float frequency, float sampleRate) {
    sampleRate = (sampleRate > 1) ? sampleRate : 44100;
    return 1.0f - expf(-2.0f * M_PI * (frequency / sampleRate));
}

/// @brief Calculates low-pass filtered value, and updates prevOut.
/// @param in Input value.
/// @param coeff Filter-coefficient.
/// @param prevOut Pointer to previous output value, which is updated to the new out.
inline float lowPassFilter(float in, float coeff, float* prevOut) {
    *prevOut = ((in - *prevOut) * coeff) + *prevOut;
    return *prevOut;
}

/// @brief Calculates high-pass filtered value, and updates prevOut.
/// @param in Input value.
/// @param coeff Filter-coefficient.
/// @param prevOut Pointer to previous output value, which is updated to the new out.
inline float highPassFilter(float in, float coeff, float* prevOut) {
    *prevOut = ((in - *prevOut) * coeff) + *prevOut;
    return in - *prevOut;
}


//-----------------------------------------------------------------------------
// Snippets
//-----------------------------------------------------------------------------
//channels = std::max(inputs[DEC_MID_INPUT].getChannels(), inputs[DEC_SIDE_INPUT].getChannels());
//float pitch_0 = 1.f + std::round(params[OCTAVE_PARAM].getValue()) + params[TUNE_PARAM].getValue() / 12.f;
//configParam(RATE_PARAM, std::log2(0.002f), std::log2(2000.f), std::log2(2.f), "Clock rate", " Hz", 2);
//float steps = std::ceil(std::pow(shape, 2) * 15 + 1);
//v = std::fmin(phase * p, 1.f);
//v = std::cos(M_PI * v);//v = rescale(v, 0.f, 1.f, lastVoltage, nextVoltage);
//random::uniform()
/*
void randomizeValues(int channel) {
		for (int i = 0; i < 7; i++) {
			values[i][channel] = random::get<float>() * randomGain + randomOffset;
		}
	}
*/
//float values[7][16] = {};   // 7 random numbers for 16 channels
//rand = clamp(rand, 0.f, 1.f);

//math.h
//#define M_PI		3.14159265358979323846
//#define M_PI_2		1.57079632679489661923
//#define M_PI_4		0.78539816339744830962
//#define M_1_PI		0.31830988618379067154
//#define M_2_PI		0.63661977236758134308
//#define M_2_SQRTPI	1.12837916709551257390
//#define M_SQRT2		1.41421356237309504880
//#define M_SQRT1_2	0.70710678118654752440