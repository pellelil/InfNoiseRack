// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inComponents.hpp"
#include "inMath.hpp"

struct Slew2Module : InfNoiseModule {
	enum ParamId {
		RISE_TIME_PARAM,
		FALL_TIME_PARAM,
		RISE_SHAPE_PARAM,
		FALL_SHAPE_PARAM,
		RISE_TIME_TRIM_PARAM,
		FALL_TIME_TRIM_PARAM,
		RISE_SHAPE_TRIM_PARAM,
		FALL_SHAPE_TRIM_PARAM,
		TIME_SCALE_PARAM,
		TIME_LINK_PARAM,
		SHAPE_LINK_PARAM,
		SNAP_GATE_TRIG_PARAM,
		TIME_VOCT_PARAM,
		A_FOLLOW_PARAM,
		B_FOLLOW_PARAM,
		SCALE_PARAM,
		OFFSET_PARAM,
		ORDER_PARAM,
		ATT_RNG_PARAM,
		PARAMS_LEN
	};
	enum InputsId {
		RISE_TIME_INPUT,
		FALL_TIME_INPUT,
		RISE_SHAPE_INPUT,
		FALL_SHAPE_INPUT,
		A_INPUT,
		B_INPUT,
		SNAP_INPUT,
		INPUTS_LEN
	};
	enum OutputsId {
		A_SLEW_OUTPUT,
		B_SLEW_OUTPUT,
		B_CATCH_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(PROCQUAL_LIGHT, 2),
		ENUMS(CLIP_RANGE_LIGHT, 2),
		RISE_SLEW_MODE_LIGHT,
		FALL_SLEW_MODE_LIGHT,
		LIGHTS_LEN
	};

	static constexpr int sectCount = 2; // 2 sections (A and B)
	static constexpr int slewCount = sectCount * PORT_MAX_CHANNELS; // 32 (2 sections * 16 channels)
	static constexpr float bCatchEnter = 0.001f; // assert B-Catch when |remaining| <= 1 mV
	static constexpr float bCatchExit = 0.010f; // deassert when |remaining| > 10 mV

	bool timeLinked = false;
	bool shapeLinked = false;
	bool shapeReversed = false;
	bool trigMode = false;
	bool voctTimeScaling = false;  // V/Oct time scaling
	infNoiseAttRngQnt::attRange attRng = infNoiseAttRngQnt::attRange::ar_1x;
	int timeScaleIdx = (int)ts_1x;
	actReqValue<infNoiseSlewMode> slewMode = actReqValue<infNoiseSlewMode>(sm_constantRate);
	actReqValue<infNoiseLinearMode> linearMode = actReqValue<infNoiseLinearMode>(lm_linear);
	float maxSeconds = 10.f;
	float riseTimeKnob = 0.f;
	float fallTimeKnob = 0.f;
	float riseTimeTrim = 0.f;
	float fallTimeTrim = 0.f;
	float riseShapeKnob = 0.f;
	float fallShapeKnob = 0.f;
	float riseShapeTrim = 0.f;
	float fallShapeTrim = 0.f;
	float knobScale = 1.f;
	float knobOffset = 0.f;
	bool offsetMode = false; // false = Scale->Offset, true = Offset->Scale
	bool riseTimeCvConn = false;
	bool fallTimeCvConn = false;
	bool riseShapeCvConn = false;
	bool fallShapeCvConn = false;
	int firstIdx = -1;
	int lastIdx = -1;
	bool haveConnections = false;
	int channels[sectCount] = { 1, 1 };
	bool inConn[sectCount] = { false, false };
	bool snapConn = false;
	bool bCatchOutConn = false;
	int followMode[sectCount] = { 0, 0 };
	bool slewOutConn[sectCount] = { false, false };
	infNoiseSlew slew[slewCount];
	dsp::SchmittTrigger catchTrigger[sectCount][PORT_MAX_CHANNELS];
	bool bCaught[PORT_MAX_CHANNELS] = {}; // B-Catch hysteresis latch per channel

	Slew2Module() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
		configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));
		configLight(RISE_SLEW_MODE_LIGHT, "Rise slew mode (dim=constant rate, blue=constant time)");
		configLight(FALL_SLEW_MODE_LIGHT, "Fall slew mode (dim=constant rate, blue=constant time)");

		configParam<infNoiseSqTimeQnt>(RISE_TIME_PARAM, 0.f, 1.f, 0.f, "Rise time (0 to 10 s)", " s");
		configParam<infNoiseSqTimeQnt>(FALL_TIME_PARAM, 0.f, 1.f, 0.f, "Fall time (0 to 10 s)", " s");
		configParam(RISE_SHAPE_PARAM, -1.f, 1.f, 0.f, "Rise shape (Exp/Lin/Log)");
		configParam(FALL_SHAPE_PARAM, -1.f, 1.f, 0.f, "Fall shape (Exp/Lin/Log)");
		configParam(RISE_TIME_TRIM_PARAM, -1.f, 1.f, 0.f, "Rise time CV-trim", "%", 0, 100);
		configParam(FALL_TIME_TRIM_PARAM, -1.f, 1.f, 0.f, "Fall time CV-trim", "%", 0, 100);
		configParam(RISE_SHAPE_TRIM_PARAM, -1.f, 1.f, 0.f, "Rise shape CV-trim", "%", 0, 100);
		configParam(FALL_SHAPE_TRIM_PARAM, -1.f, 1.f, 0.f, "Fall shape CV-trim", "%", 0, 100);
		configSwitch(TIME_SCALE_PARAM, 0.f, 2.f, 1.f, "Time scale", infNoiseTimeScaleMenuNames());
		configSwitch(TIME_LINK_PARAM, 0.f, 1.f, 0.f, "Time link", { "Disabled", "Enabled" });
		configSwitch(SHAPE_LINK_PARAM, 0.f, 2.f, 0.f, "Fall shape", { "Link disabled", "Link enabled", "Link reversed" });
		configSwitch(SNAP_GATE_TRIG_PARAM, 0.f, 1.f, 1.f, "Snap Gate/Trigger", { "Trigger when red", "Gate when green" });
		configSwitch(TIME_VOCT_PARAM, 0.f, 1.f, 0.f, "V/Oct time scaling", { "Disabled", "Enabled" });
		configSwitch(A_FOLLOW_PARAM, 0.f, 2.f, 0.f, "A envelope follower", { "Disabled", "Abs", "Cut negatives" });
		configSwitch(B_FOLLOW_PARAM, 0.f, 2.f, 0.f, "B envelope follower", { "Disabled", "Abs", "Cut negatives" });
		configParam<infNoiseAttRngQnt>(SCALE_PARAM, -1.f, 1.f, 1.f, "Scale (-1x to +1x)", " x", 0, 1);
		configParam(OFFSET_PARAM, -10.f, 10.f, 0.f, "Offset (-10V to +10V)", " V");
		configSwitch(ORDER_PARAM, 0.f, 1.f, 0.f, "Order", { "Scale->Offset", "Offset->Scale" });
		configSwitch(ATT_RNG_PARAM, 0.f, 3.f, 0.f, "Scale-range", { "1x", "2x", "5x", "10x" });

		configInput(RISE_TIME_INPUT, "Rise time CV");
		configInput(FALL_TIME_INPUT, "Fall time CV (normalized to Rise time CV)");
		configInput(RISE_SHAPE_INPUT, "Rise shape CV");
		configInput(FALL_SHAPE_INPUT, "Fall shape CV (normalized to Rise shape CV)");
		configInput(A_INPUT, "A");
		configInput(B_INPUT, "B (normalized to A)");
		configInput(SNAP_INPUT, "Snap");

		configOutput(A_SLEW_OUTPUT, "A-Slew");
		configOutput(B_SLEW_OUTPUT, "B-Slew");
		configOutput(B_CATCH_OUTPUT, "B-Catch");

		configBypass(A_INPUT, A_SLEW_OUTPUT);
		configBypass(B_INPUT, B_SLEW_OUTPUT);

		haveProcQuality = true;
		haveAutoProcQuality = false;
		haveOutQuantize = false;
		haveOutClipRange = true;
		haveGateDetect = true;
		haveGateHighLow = false;
		haveTrigDetect = true;
		haveTrigHighLow = false;

		ensureNormExpLogLuts();
	}

	void resetCatchTriggers() {
		for (int i = 0; i < sectCount; i++) {
			for (int c = 0; c < PORT_MAX_CHANNELS; c++)
				catchTrigger[i][c].reset();
		}
		for (int c = 0; c < PORT_MAX_CHANNELS; c++)
			bCaught[c] = false;
	}

	void onReset(const ResetEvent& e) override {
		InfNoiseModule::onReset(e);

		for (int i = 0; i < slewCount; i++)
			slew[i].reset();
		resetCatchTriggers();
		slewMode.setBoth(sm_constantRate);
		linearMode.setBoth(lm_linear);
		attRng = infNoiseAttRngQnt::attRange::ar_1x;
		params[SCALE_PARAM].setValue(1.f);
	}

	void dataFromJson(json_t* rootJ) override {
		InfNoiseModule::dataFromJson(rootJ);

		float lastOut[slewCount];
		getJsonFloatArray(rootJ, "lastOut", lastOut, slewCount, 0.f);
		for (int i = 0; i < slewCount; i++)
			slew[i].snap(lastOut[i]);
		slewMode.setBoth((infNoiseSlewMode)getJsonInt(rootJ, "slewMode", (int)sm_constantRate));
		int loadedLinear = getJsonInt(rootJ, "linearMode", -1);
		if (loadedLinear < 0)
			loadedLinear = getJsonBool(rootJ, "useSCurve", false) ? (int)lm_sCurve : (int)lm_linear;
		linearMode.setBoth((infNoiseLinearMode)loadedLinear);
	}

	void dataToJson(json_t* rootJ) override {
		float lastOut[slewCount] = {};
		for (int i = 0; i < slewCount; i++)
			lastOut[i] = slew[i].last();
		setJsonFloatArray(rootJ, "lastOut", lastOut, slewCount);
		json_object_set_new(rootJ, "slewMode", json_integer((int)slewMode.req));
		json_object_set_new(rootJ, "linearMode", json_integer((int)linearMode.req));
	}

	void processParams(const ProcessArgs& args) {
		preProcessParams(args);
		//--------------------

		voctTimeScaling = params[TIME_VOCT_PARAM].getValue() > 0.5f;

		int rngIdx = (int)(params[ATT_RNG_PARAM].getValue() + 0.5f);
		if (rngIdx < 0)
			rngIdx = 0;
		if (rngIdx > 3)
			rngIdx = 3;
		infNoiseAttRngQnt::attRange newRng = (infNoiseAttRngQnt::attRange)rngIdx;
		if (newRng != attRng || mustProcessParams) {
			attRng = newRng;
			infNoiseAttRngQnt* attQty = dynamic_cast<infNoiseAttRngQnt*>(paramQuantities[SCALE_PARAM]);
			if (attQty)
				attQty->setRange(attRng, "Scale");
		}

		// Handle time link
		timeLinked = params[TIME_LINK_PARAM].getValue() > 0.5f;
		if (timeLinked) {
			params[FALL_TIME_PARAM].setValue(params[RISE_TIME_PARAM].getValue());
			params[FALL_TIME_TRIM_PARAM].setValue(params[RISE_TIME_TRIM_PARAM].getValue());
		}

		// Handle shape link
		float shapeLink = params[SHAPE_LINK_PARAM].getValue();
		shapeLinked = shapeLink > 0.5f;
		shapeReversed = shapeLink > 1.5f;
		riseShapeKnob = params[RISE_SHAPE_PARAM].getValue();
		if (shapeLinked) {
			float linkedShape = shapeReversed ? -riseShapeKnob : riseShapeKnob;
			params[FALL_SHAPE_PARAM].setValue(linkedShape);
			params[FALL_SHAPE_TRIM_PARAM].setValue(params[RISE_SHAPE_TRIM_PARAM].getValue());
			fallShapeKnob = linkedShape;
		}
		else {
			fallShapeKnob = params[FALL_SHAPE_PARAM].getValue();
		}

		// Read knob values
		riseTimeKnob = params[RISE_TIME_PARAM].getValue();
		fallTimeKnob = params[FALL_TIME_PARAM].getValue();
		riseTimeTrim = params[RISE_TIME_TRIM_PARAM].getValue();
		fallTimeTrim = params[FALL_TIME_TRIM_PARAM].getValue();
		riseShapeTrim = params[RISE_SHAPE_TRIM_PARAM].getValue();
		fallShapeTrim = params[FALL_SHAPE_TRIM_PARAM].getValue();
		static const float rangeFactors[4] = { 1.f, 2.f, 5.f, 10.f };
		knobScale = params[SCALE_PARAM].getValue() * rangeFactors[(int)attRng];
		knobOffset = params[OFFSET_PARAM].getValue();
		offsetMode = params[ORDER_PARAM].getValue() > 0.5f;

		// Set maxSeconds based on time scale, and apply scale
		int tsIdx = (int)(params[TIME_SCALE_PARAM].getValue() + 0.5f);
		if (tsIdx < 0)
			tsIdx = 0;
		if (tsIdx >= (int)ts_len)
			tsIdx = (int)ts_len - 1;
		if (tsIdx != timeScaleIdx || mustProcessParams) {
			timeScaleIdx = tsIdx;
			infNoiseTimeScale scale = (infNoiseTimeScale)tsIdx;
			applyInfNoiseSqTimeQntScale(paramQuantities[RISE_TIME_PARAM], "Rise time", scale);
			applyInfNoiseSqTimeQntScale(paramQuantities[FALL_TIME_PARAM], "Fall time", scale);
			infNoiseSqTimeQnt* tq = dynamic_cast<infNoiseSqTimeQnt*>(paramQuantities[RISE_TIME_PARAM]);
			if (tq)
				maxSeconds = tq->maxSeconds;
		}

		if (slewMode.needsUpdate() || mustProcessParams) {
			slewMode.updateActual();
			for (int i = 0; i < slewCount; i++)
				slew[i].setMode(slewMode.act);
			float modeLight = (slewMode.act == sm_constantTime) ? 1.f : 0.f;
			lights[RISE_SLEW_MODE_LIGHT].setBrightness(modeLight);
			lights[FALL_SLEW_MODE_LIGHT].setBrightness(modeLight);
		}

		if (linearMode.needsUpdate() || mustProcessParams) {
			linearMode.updateActual();
			for (int i = 0; i < slewCount; i++)
				slew[i].setLinearMode(linearMode.act);
		}

		// Detect catch gate/trigger mode
		bool newTrigMode = params[SNAP_GATE_TRIG_PARAM].getValue() < 0.5f;
		if (newTrigMode != trigMode) {
			trigMode = newTrigMode;
			resetCatchTriggers();
		}

		// Detect CV connections
		riseTimeCvConn = inputs[RISE_TIME_INPUT].isConnected();
		fallTimeCvConn = inputs[FALL_TIME_INPUT].isConnected();
		riseShapeCvConn = inputs[RISE_SHAPE_INPUT].isConnected();
		fallShapeCvConn = inputs[FALL_SHAPE_INPUT].isConnected();

		haveConnections = false;
		firstIdx = -1;
		lastIdx = -1;
		snapConn = inputs[SNAP_INPUT].isConnected();
		bCatchOutConn = outputs[B_CATCH_OUTPUT].isConnected();
		for (int i = 0; i < sectCount; i++) {
			inConn[i] = inputs[A_INPUT + i].isConnected();
			followMode[i] = (params[A_FOLLOW_PARAM + i].getValue() > 1.5f) ? 2
				: (params[A_FOLLOW_PARAM + i].getValue() > 0.5f) ? 1 : 0;
			if (inConn[i])
				channels[i] = std::max(inputs[A_INPUT + i].getChannels(), 1);
			else if (i == 1 && inConn[0])
				channels[i] = channels[0]; // B normalizes to A
			else
				channels[i] = 1;
			slewOutConn[i] = outputs[A_SLEW_OUTPUT + i].isConnected();
			outputs[A_SLEW_OUTPUT + i].setChannels(channels[i]);
			bool sectionUsed = inConn[i] || snapConn || slewOutConn[i] || (i == 1 && bCatchOutConn);
			if (sectionUsed) {
				if (firstIdx < 0)
					firstIdx = i;
				lastIdx = i;
				haveConnections = true;
			}
		}
		if (bCatchOutConn)
			outputs[B_CATCH_OUTPUT].setChannels(channels[1]);

		//--------------------
		postProcessParams(args);
	}

	void process(const ProcessArgs& args) override {
		bool doProcessParams = mustProcessParams ||
			((cycle256 & patternProcessParams) == patternProcessParams);
		if (doProcessParams)
			processParams(args);

		bool doProcess = (doProcessParams ||
			((cycle256 & processQualityPatterns[procQuality.act]) == processQualityPatterns[procQuality.act]));

		if (doProcess && haveConnections) {
			for (int i = firstIdx; i <= lastIdx; i++) {
				int baseIdx = i * PORT_MAX_CHANNELS;
				for (int c = 0; c < channels[i]; c++) {
					// Read input voltage (B normalizes to A)
					float inVolt = 0.f;
					if (inConn[i])
						inVolt = inputs[A_INPUT + i].getVoltage(c);
					else if (i == 1 && inConn[0])
						inVolt = inputs[A_INPUT].getPolyVoltage(c);
					if (followMode[i] == 1)
						inVolt = std::fabs(inVolt);
					else if (followMode[i] == 2 && inVolt < 0.f)
						inVolt = 0.f;
					inVolt = offsetMode
						? (inVolt + knobOffset) * knobScale
						: inVolt * knobScale + knobOffset;

					// Read/scale rise/fall times
					float riseTimeCv = inputs[RISE_TIME_INPUT].getPolyVoltage(c);
					float riseK = riseTimeKnob;
					if (!voctTimeScaling && riseTimeCvConn)
						riseK += riseTimeTrim * riseTimeCv / 10.f;
					riseK = clamp(riseK, 0.f, 1.f);
					float riseSec = riseK * riseK * maxSeconds;
					if (voctTimeScaling && riseTimeCvConn)
						riseSec *= dsp::exp2_taylor5(-riseTimeTrim * riseTimeCv);

					float fallSec;
					if (timeLinked) {
						fallSec = riseSec;
					}
					else {
						float fallTimeCv = fallTimeCvConn
							? inputs[FALL_TIME_INPUT].getPolyVoltage(c)
							: riseTimeCv;
						float fallK = fallTimeKnob;
						bool fallCv = fallTimeCvConn || riseTimeCvConn;
						if (!voctTimeScaling && fallCv)
							fallK += fallTimeTrim * fallTimeCv / 10.f;
						fallK = clamp(fallK, 0.f, 1.f);
						fallSec = fallK * fallK * maxSeconds;
						if (voctTimeScaling && fallCv)
							fallSec *= dsp::exp2_taylor5(-fallTimeTrim * fallTimeCv);
					}

					// Read rise/fall shapes
					float riseShapeCv = inputs[RISE_SHAPE_INPUT].getPolyVoltage(c);
					float riseShape = riseShapeKnob;
					if (riseShapeCvConn)
						riseShape += riseShapeTrim * riseShapeCv / 10.f;
					riseShape = clamp(riseShape, -1.f, 1.f);

					float fallShape;
					if (shapeLinked) {
						fallShape = shapeReversed ? -riseShape : riseShape;
					}
					else {
						float fallShapeCv = fallShapeCvConn
							? inputs[FALL_SHAPE_INPUT].getPolyVoltage(c)
							: riseShapeCv;
						fallShape = fallShapeKnob;
						if (fallShapeCvConn || riseShapeCvConn)
							fallShape += fallShapeTrim * fallShapeCv / 10.f;
						fallShape = clamp(fallShape, -1.f, 1.f);
					}

					// Update slew engine (times and shapes)
					int idx = baseIdx + c;
					slew[idx].setTimes(riseSec, fallSec);
					slew[idx].setShapes(riseShape, fallShape);

					// Snap both sections from the shared Snap input
					if (snapConn) {
						float snapVolt = inputs[SNAP_INPUT].getPolyVoltage(c);
						if (trigMode) {
							if (catchTrigger[i][c].process(snapVolt,
								trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act]))
								slew[idx].snap(inVolt);
						}
						else if (snapVolt >= trueDetectValues[gateDetHigh.act]) {
							slew[idx].snap(inVolt);
						}
					}

					// Update slew output
					float slewVolt = clipToVoltRange(slew[idx].next(inVolt, procSampleTime), outClipRange.act);
					if (slewOutConn[i])
						outputs[A_SLEW_OUTPUT + i].setVoltage(slewVolt, c);

					// B-Catch: enter at 1 mV, exit at 10 mV (hysteresis)
					// (B input, or A if B is unpatched — already used by next())
					if (i == 1 && bCatchOutConn) {
						if (bCaught[c])
							bCaught[c] = slew[idx].within(bCatchExit);
						else
							bCaught[c] = slew[idx].within(bCatchEnter);
						outputs[B_CATCH_OUTPUT].setVoltage(
							bCaught[c] ? voltValues[gateOutHigh.act] : voltValues[gateOutLow.act], c);
					}
				}
			}
		}

		cycle256++;
	}
};

struct Slew2ModuleWidget : InfNoiseModuleWidget {
	InfNoiseDisableOverlayGroup* fallTimeLinkOverlayGroup = nullptr;
	InfNoiseDisableOverlayGroup* fallShapeLinkOverlayGroup = nullptr;
	bool timeLinked = false;
	bool shapeLinked = false;

	Slew2ModuleWidget(Slew2Module* module) {
		initializeWidget(module, "res/Slew2");

		const float leftCol = 15.0f;
		const float rightCol = 45.0f;
		const float centerCol = 30.0f;

		addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_green, bc_black, bc_red>>(
			Vec(centerCol, 36.212f), module, Slew2Module::TIME_SCALE_PARAM));
		addParam(createParamCentered<infNoiseLtSmallButton<bc_green, false>>(Vec(centerCol, 48.801f), module, Slew2Module::TIME_LINK_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(leftCol, 56.655f), module, Slew2Module::RISE_TIME_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(rightCol, 56.655f), module, Slew2Module::FALL_TIME_PARAM));
		addChild(createLightCentered<TinyLight<BlueLight>>(Vec(leftCol - 10.f, 56.655f - 10.f), module, Slew2Module::RISE_SLEW_MODE_LIGHT));
		addChild(createLightCentered<TinyLight<BlueLight>>(Vec(rightCol + 10.f, 56.655f - 10.f), module, Slew2Module::FALL_SLEW_MODE_LIGHT));
		addParam(createParamCentered<Trimpot>(Vec(leftCol, 84.395f), module, Slew2Module::RISE_TIME_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(Vec(rightCol, 84.395f), module, Slew2Module::FALL_TIME_TRIM_PARAM));
		addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftCol, 112.483f), module, Slew2Module::RISE_TIME_INPUT));
		addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(rightCol, 112.483f), module, Slew2Module::FALL_TIME_INPUT));
		addParam(createParamCentered<infNoiseLtSmallButton<bc_green, false>>(Vec(centerCol, 104.280f), module, Slew2Module::TIME_VOCT_PARAM));

		addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_green, bc_red>>(
			Vec(centerCol, 137.456f), module, Slew2Module::SHAPE_LINK_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(leftCol, 145.310f), module, Slew2Module::RISE_SHAPE_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(rightCol, 145.310f), module, Slew2Module::FALL_SHAPE_PARAM));
		addParam(createParamCentered<Trimpot>(Vec(leftCol, 173.050f), module, Slew2Module::RISE_SHAPE_TRIM_PARAM));
		addParam(createParamCentered<Trimpot>(Vec(rightCol, 173.050f), module, Slew2Module::FALL_SHAPE_TRIM_PARAM));
		addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftCol, 201.138f), module, Slew2Module::RISE_SHAPE_INPUT));
		addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(rightCol, 201.138f), module, Slew2Module::FALL_SHAPE_INPUT));

		addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_green, bc_yellow, bc_red>>(
			Vec(25.465f, 219.499f), module, Slew2Module::ATT_RNG_PARAM));
		addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_blue>>(
			Vec(33.694f, 219.499f), module, Slew2Module::ORDER_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(leftCol, 233.005f), module, Slew2Module::SCALE_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(Vec(rightCol, 233.005f), module, Slew2Module::OFFSET_PARAM));

		const float followBtnY = 259.449f;
		addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_green, bc_red>>(
			Vec(24.843f, followBtnY), module, Slew2Module::A_FOLLOW_PARAM));
		addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_black, bc_green, bc_red>>(
			Vec(55.249f, followBtnY), module, Slew2Module::B_FOLLOW_PARAM));
		addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftCol, 271.888f), module, Slew2Module::A_INPUT));
		addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(rightCol, 271.888f), module, Slew2Module::B_INPUT));
		addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_red, bc_green>>(
			Vec(24.646f, 290.383f), module, Slew2Module::SNAP_GATE_TRIG_PARAM));
		addInput(createInputCentered<infNoiseThemedPolyPort>(Vec(leftCol, 303.090f), module, Slew2Module::SNAP_INPUT));
		addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightCol, 303.090f), module, Slew2Module::B_CATCH_OUTPUT));
		addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(leftCol, 334.947f), module, Slew2Module::A_SLEW_OUTPUT));
		addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(rightCol, 334.947f), module, Slew2Module::B_SLEW_OUTPUT));

		InfNoiseDisableOverlayManager& overlayManager = getDisableOverlayManager();
		fallTimeLinkOverlayGroup = overlayManager.addGroup("Fall time linked to Rise time");
		fallTimeLinkOverlayGroup->addTargets(InfNoiseOverlayTargetType::param, {
			Slew2Module::FALL_TIME_PARAM,
			Slew2Module::FALL_TIME_TRIM_PARAM
		});
		fallShapeLinkOverlayGroup = overlayManager.addGroup("Fall shape linked to Rise shape");
		fallShapeLinkOverlayGroup->addTargets(InfNoiseOverlayTargetType::param, {
			Slew2Module::FALL_SHAPE_PARAM,
			Slew2Module::FALL_SHAPE_TRIM_PARAM
		});
	}

	void step() override {
		InfNoiseModuleWidget::step();

		if (!module)
			return;

		auto* m = static_cast<Slew2Module*>(module);
		if (fallTimeLinkOverlayGroup && m->timeLinked != timeLinked) {
			timeLinked = m->timeLinked;
			fallTimeLinkOverlayGroup->setActive(timeLinked);
		}
		if (fallShapeLinkOverlayGroup && m->shapeLinked != shapeLinked) {
			shapeLinked = m->shapeLinked;
			fallShapeLinkOverlayGroup->setActive(shapeLinked);
		}
	}

	void appendContextMenu(Menu* menu) override {
		InfNoiseModuleWidget::appendContextMenu(menu);
		Slew2Module* module = dynamic_cast<Slew2Module*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexPtrSubmenuItem("Slew mode", infNoiseSlewModeMenuNames(), &module->slewMode.req));
		menu->addChild(createIndexPtrSubmenuItem("Linear mode", infNoiseLinearModeMenuNames(), &module->linearMode.req));

		appendInfNoiseMenuItems(menu);
	}
};

Model* modelSlew2 = createModel<Slew2Module, Slew2ModuleWidget>("Slew2");
