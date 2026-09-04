// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#pragma once

#include "plugin.hpp"
#include "inMath.hpp"
#include "inUtil.hpp"

/// Shared base for ADR Envelope and ADSDR Envelope (not a Rack Model).
struct InfNoiseEnvelopeModule : InfNoiseModule {
	// Attack/Decay/Release are ramps; Sustain/Delay/Idle are holds.
	// ADR never enters ep_decay; ADR sustain is hold at A.level.
	// Default is ep_idle (idle at R.level, waiting for attack).
	enum envPhase {
		ep_attack,   // ramping to A.level
		ep_decay,    // ramping to Sustain (ADSDR only)
		ep_sustain,  // held at Sustain (ADSDR) / A.level (ADR)
		ep_delay,    // delay hold
		ep_release,  // ramping to R.level
		ep_idle,     // held at R.level
		ep_len
	};

	enum envMotionType {
		em_steady,
		em_rise,
		em_fall
	};

	envPhase phase = ep_idle;
	float phasePos = 0.f;
	float envelope = 0.f;
	float prevEnvelope = 0.f;
	envMotionType envMotion = em_steady;
	static const int envMotionSteadyHold = 3;
	int envMotionSteadyCount = 0;
	// GreenRed phase lights (ADSDR). ADR never enters ep_decay, so those entries stay unused.
	float attackLightGreen[ep_len] = { 1.f, 1.f, 0.f, 0.f, 0.f, 0.f };
	float attackLightRed[ep_len]   = { 0.f, 1.f, 1.f, 0.f, 0.f, 0.f };
	float releaseLightGreen[ep_len] = { 0.f, 0.f, 0.f, 1.f, 1.f, 0.f };
	float releaseLightRed[ep_len]   = { 0.f, 0.f, 0.f, 1.f, 0.f, 1.f };

	void onReset(const ResetEvent& e) override {
		InfNoiseModule::onReset(e);
		phase = ep_idle;
		phasePos = 0.f;
		envMotion = em_steady;
		envMotionSteadyCount = 0;
	}

	void dataFromJson(json_t* rootJ) override {
		InfNoiseModule::dataFromJson(rootJ);
		int p = getJsonInt(rootJ, "phase", (int)ep_idle);
		phase = (envPhase)clamp(p, (int)ep_attack, (int)ep_idle);
		phasePos = getJsonFloat(rootJ, "phasePos", 0.f);
		envelope = getJsonFloat(rootJ, "envelope", 0.f);
		prevEnvelope = envelope;
		envMotion = em_steady;
		envMotionSteadyCount = 0;
	}

	void dataToJson(json_t* rootJ) override {
		json_object_set_new(rootJ, "phase", json_integer((int)phase));
		json_object_set_new(rootJ, "phasePos", json_real(phasePos));
		json_object_set_new(rootJ, "envelope", json_real(envelope));
	}

	void updateEnvMotion() {
		float delta = envelope - prevEnvelope;
		if (std::fabs(delta) < 1e-10f) {
			if (envMotion != em_steady) {
				envMotionSteadyCount++;
				if (envMotionSteadyCount >= envMotionSteadyHold)
					envMotion = em_steady;
			}
		}
		else {
			envMotionSteadyCount = 0;
			envMotion = (delta > 0.f) ? em_rise : em_fall;
		}
	}

	/// Push phase/motion to adjacent Envelope Phase Expander modules (defined in EnvelopePhaseExpander.cpp).
	void pushToExpanders();

	float readTimeParam(int paramId, float sampleTime) {
		float t = getParamQuantity(paramId)->getDisplayValue();
		return (t < sampleTime) ? 0.f : t;
	}

	// Mix linear with normExp (shape < 0) or normLog (shape > 0). Fill-time only.
	static float applyShape(float x, float shape) {
		if (shape < 0.f)
			return x + (-shape) * (normExp(x) - x);
		if (shape > 0.f)
			return x + shape * (normLog(x) - x);
		return x;
	}

	// Forward LUT: shaped 0→1 vs phasePos. Inverse: phasePos vs shaped (interrupt).
	static void rebuildShapeLuts(float shape, Lut1D<256>& fwd, Lut1D<256>& inv) {
		const int n = Lut1D<256>::Size;
		for (int i = 0; i < n; i++) {
			float x = (float)i / (float)(n - 1);
			fwd.data[i] = applyShape(x, shape);
		}
		fwd.data[0] = 0.f;
		fwd.data[n - 1] = 1.f;

		int lo = 0;
		for (int j = 0; j < n; j++) {
			float y = (float)j / (float)(n - 1);
			while (lo < n - 1 && fwd.data[lo + 1] < y)
				lo++;
			float y0 = fwd.data[lo];
			float y1 = fwd.data[lo + 1];
			float x0 = (float)lo / (float)(n - 1);
			float x1 = (float)(lo + 1) / (float)(n - 1);
			float denom = y1 - y0;
			float frac = (denom > 1e-12f) ? (y - y0) / denom : 0.f;
			inv.data[j] = x0 + frac * (x1 - x0);
		}
		inv.data[0] = 0.f;
		inv.data[n - 1] = 1.f;
	}
};
