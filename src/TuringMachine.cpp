// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#include "plugin.hpp"
#include "inMath.hpp"
#include "inComponents.hpp"
#include "inUtil.hpp"

struct TuringMachineModule : InfNoiseModule {
    enum ParamId {
        BIT_CHANGE_PARAM,
        LOCK_BTN_PARAM,
        LOCK_BTN_LATCH_PARAM,
        LOCK_SWITCH_PARAM,
        CHANGE_PROB_PARAM,
        CHANGE_PROB_TRIM_PARAM,
        BIT_PROB_PARAM,
        BIT_PROB_TRIM_PARAM,
        LENGTH_PARAM,
        LENGTH_TRIM_PARAM,
        RANGE_PARAM,
        RANGE_TRIM_PARAM,
        MINCNTRMAX_PARAM,
        MINCNTRMAX_TRIM_PARAM,
        MINCNTRMAX_BTN_PARAM,
        RESET_BTN_PARAM,
        RESET_SWITCH_PARAM,
        CLOCK_BTN_PARAM,
        ROTATE_BTN_PARAM,
        BIT1_TRIG_GATE_PARAM,
        BIT2_TRIG_GATE_PARAM,
        BIT3_TRIG_GATE_PARAM,
        BIT4_TRIG_GATE_PARAM,
        BIT5_TRIG_GATE_PARAM,
        BIT6_TRIG_GATE_PARAM,
        BIT7_TRIG_GATE_PARAM,
        BIT8_TRIG_GATE_PARAM,
        BIT12_TRIG_GATE_PARAM,
        BIT13_TRIG_GATE_PARAM,
        BIT14_TRIG_GATE_PARAM,
        BIT15_TRIG_GATE_PARAM,
        BIT23_TRIG_GATE_PARAM,
        BIT24_TRIG_GATE_PARAM,
        BIT34_TRIG_GATE_PARAM,
        BIT47_TRIG_GATE_PARAM,
        PARAMS_LEN
    };
    enum InputsId {
        LOCK_INPUT,
        CHANGE_PROB_INPUT,
        BIT_PROB_INPUT,
        LENGTH_INPUT,
        RANGE_INPUT,
        MINCNTRMAX_INPUT,
        RESET_INPUT,
        CLOCK_INPUT,
        ROTATE_INPUT,
        INPUTS_LEN
    };
    enum OutputsId {
        BINARY_OUTPUT,
        FIXED_OUTPUT,
        BIT1_OUTPUT,
        BIT2_OUTPUT,
        BIT3_OUTPUT,
        BIT4_OUTPUT,
        BIT5_OUTPUT,
        BIT6_OUTPUT,
        BIT7_OUTPUT,
        BIT8_OUTPUT,
        BIT12_OUTPUT,
        BIT13_OUTPUT,
        BIT14_OUTPUT,
        BIT15_OUTPUT,
        BIT23_OUTPUT,
        BIT24_OUTPUT,
        BIT34_OUTPUT,
        BIT47_OUTPUT,
        PULSES_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        ENUMS(PROCQUAL_LIGHT,2),
        ENUMS(CLIP_RANGE_LIGHT, 2),
        BIT1_LIGHT,
        BIT2_LIGHT,
        BIT3_LIGHT,
        BIT4_LIGHT,
        BIT5_LIGHT,
        BIT6_LIGHT,
        BIT7_LIGHT,
        BIT8_LIGHT,
        LOCK_LIGHT,
        MIN_LIGHT,  
        CNTR_LIGHT,
        MAX_LIGHT,
        LIGHTS_LEN
    };

    actReqValue<bool> locked = actReqValue<bool>(false);
    enum lockModeType { lm_Both, lm_Change, lm_Rotate };
    actReqValue<lockModeType> lockMode = actReqValue<lockModeType>(lm_Change);
    actReqValue<bool> lockPreventsReset = actReqValue<bool>(false);
    enum minCntrMaxType { mcm_Min, mcm_Center, mcm_Max };
    actReqValue<minCntrMaxType> minCntrMax = actReqValue<minCntrMaxType>(minCntrMaxType::mcm_Center);
    bool havePulse18 = false; // True if any pulse output 1 - 8 are used (single bit pulses)
    bool havePulse916 = false; // True if any pulse output 9 - 16 are used (anded bit pulses)
    uint32_t bit32 = 0; // 32-bit pattern (full pattern)
    uint32_t bit8 = 0; // Low 8-bit taken from bit32 (some bits might be "repeated" if length < 8)
    uint32_t inUseBits[32]; // Bits in use pattern based on length (1-32, but 1 and 2 never used)
    uint32_t notInUseBits[32]; // Bits not in use pattern based on length (1-32, but 1 and 2 never used)
    int prevLength = -1; // ensure bit8 is updated when length is changed
    bool updateBit8Lights = true; // Ensure bit8 lights are updated when needed
    dsp::SchmittTrigger lockTrigger;
    dsp::SchmittTrigger minCntrMaxTrigger;
    dsp::SchmittTrigger resetTrigger;
    dsp::SchmittTrigger clockTrigger;
    dsp::SchmittTrigger rotateTrigger;
    dsp::SchmittTrigger pulseInTrigger[16] = {
        dsp::SchmittTrigger(),
        dsp::SchmittTrigger(),
        dsp::SchmittTrigger(),
        dsp::SchmittTrigger(),
        dsp::SchmittTrigger(),
        dsp::SchmittTrigger(),
        dsp::SchmittTrigger(),
        dsp::SchmittTrigger(),
        dsp::SchmittTrigger(),
        dsp::SchmittTrigger(),
        dsp::SchmittTrigger(),
        dsp::SchmittTrigger(),
        dsp::SchmittTrigger(),
        dsp::SchmittTrigger(),
        dsp::SchmittTrigger(),
        dsp::SchmittTrigger()
    };
    infNoiseOutTrigger pulseOutTrigger[16] = {
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f),
        infNoiseOutTrigger(1e-3f, 1e-3f)
    };

	TuringMachineModule() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configLight(PROCQUAL_LIGHT, processQualityNames[procQuality.act]);
        configLight(CLIP_RANGE_LIGHT, getClipRangeLightName(outClipRange.act));

        // Bit lights, bit-change lights and Bit-change switch
        for (int i = 0; i < 8; i++) {
			configLight(BIT1_LIGHT + i, string::f("Bit-%d", i + 1));
		}
        configSwitch(BIT_CHANGE_PARAM, 0.0f, 1.0f, 0.0f, "Lock", { "Bit-1", "Random bit" });

        // Lock controls
        configLight(LOCK_LIGHT, "Locked if lit");
        configSwitch(LOCK_BTN_PARAM, 0.0f, 1.0f, 0.0f, "Lock");
        configSwitch(LOCK_BTN_LATCH_PARAM, 0.0f, 1.0f, 1.0f, "Lock-latch", { "Unlatched", "Latched" });
        configSwitch(LOCK_SWITCH_PARAM, 0.0f, 1.0f, 0.0f, "Lock-mode", { "Gate", "Trigger" });
        configInput(LOCK_INPUT, "Lock");

        // Change probability controls
        configParam(CHANGE_PROB_PARAM, 0.f, 1.f, 0.5f, "Change probability (0% to 100%)", " %", 0, 100);
        configParam(CHANGE_PROB_TRIM_PARAM, -1.f, 1.f, 0.f, "Change prob. CV-trim", "%", 0, 100);
        configInput(CHANGE_PROB_INPUT, "Change probability");

        // Bit probability controls
        configParam(BIT_PROB_PARAM, -1.f, 1.f, 0.0f, "Bit probability ([0] [INV] [1])", " %", 0, 100);
        configParam(BIT_PROB_TRIM_PARAM, -1.f, 1.f, 0.f, "Bit prob. CV-trim", "%", 0, 100);
        configInput(BIT_PROB_INPUT, "Bit probability");

        // Length controls
        configSwitch(LENGTH_PARAM, 3.0f, 32.0f, 8.0f, "Length", {"3 bits", "4 bits", "5 bits", "6 bits", "7 bits", 
            "8 bits", "9 bits", "10 bits", "11 bits", "12 bits", "13 bits", "14 bits", "15 bits", "16 bits", 
            "17 bits", "18 bits", "19 bits", "20 bits", "21 bits", "22 bits", "23 bits", "24 bits", "25 bits", 
            "26 bits", "27 bits", "28 bits", "29 bits", "30 bits", "31 bits", "32 bits"});
        configParam(LENGTH_TRIM_PARAM, -1.f, 1.f, 0.f, "Length CV-trim", "%", 0, 100);
        configInput(LENGTH_INPUT, "Length");

        // Range controls
        configParam(RANGE_PARAM, 0.f, 10.f, 10.0f, "Range", " V", 0, 1);
        configParam(RANGE_TRIM_PARAM, -1.f, 1.f, 0.f, "Range CV-trim", "%", 0, 100);
        configInput(RANGE_INPUT, "Range");

        // Min/Center/Max controls
        configParam(MINCNTRMAX_PARAM, -10.f, 10.f, 0.0f, "Min/Center/Max", " V", 0, 1);
        configParam(MINCNTRMAX_TRIM_PARAM, -1.f, 1.f, 0.f, "Min/Center/Max CV-trim", "%", 0, 100);
        configInput(MINCNTRMAX_INPUT, "Min/Center/Max (-10V to +10V)");
        configSwitch(MINCNTRMAX_BTN_PARAM, 0.0f, 1.0f, 0.0f, "Toggle Min/Center/Max-mode");
        configLight(MIN_LIGHT, "Minimum when lit");
        configLight(CNTR_LIGHT, "Center when lit");
        configLight(MAX_LIGHT, "Maximum when lit");

        // Reset controls
        configSwitch(RESET_BTN_PARAM, 0.0f, 1.0f, 0.0f, "Reset");
        configSwitch(RESET_SWITCH_PARAM, 0.0f, 2.0f, 0.0f, "Reset-mode", { "Random", "Set all bits to 0", "Set all bit to 1" });
        configInput(RESET_INPUT, "Reset trigger");

        // Clock controls
        configInput(CLOCK_INPUT, "Clock trigger");
        configSwitch(CLOCK_BTN_PARAM, 0.0f, 1.0f, 0.0f, "Clock");

        // Rotate controls
        configInput(ROTATE_INPUT, "Rotate trigger (normalized to clock-trigger)");
        configSwitch(ROTATE_BTN_PARAM, 0.0f, 1.0f, 0.0f, "Rotate");

        // Value output
        configOutput(BINARY_OUTPUT, "Binary bit-weights");
        configOutput(FIXED_OUTPUT, "Fixed bit-weights");

        // Bit 1-8 outputs
        configOutput(BIT1_OUTPUT, "Bit-1");
        configSwitch(BIT1_TRIG_GATE_PARAM, 0.0f, 1.0f, 1.0f, "Bit-1 mode", { "Trigger", "Gate" });
        configOutput(BIT2_OUTPUT, "Bit-2");
        configSwitch(BIT2_TRIG_GATE_PARAM, 0.0f, 1.0f, 1.0f, "Bit-2 mode", { "Trigger", "Gate" });
        configOutput(BIT3_OUTPUT, "Bit-3");
        configSwitch(BIT3_TRIG_GATE_PARAM, 0.0f, 1.0f, 1.0f, "Bit-3 mode", { "Trigger", "Gate" });
        configOutput(BIT4_OUTPUT, "Bit-4");
        configSwitch(BIT4_TRIG_GATE_PARAM, 0.0f, 1.0f, 1.0f, "Bit-4 mode", { "Trigger", "Gate" });
        configOutput(BIT5_OUTPUT, "Bit-5");
        configSwitch(BIT5_TRIG_GATE_PARAM, 0.0f, 1.0f, 1.0f, "Bit-5 mode", { "Trigger", "Gate" });
        configOutput(BIT6_OUTPUT, "Bit-6");
        configSwitch(BIT6_TRIG_GATE_PARAM, 0.0f, 1.0f, 1.0f, "Bit-6 mode", { "Trigger", "Gate" });
        configOutput(BIT7_OUTPUT, "Bit-7");
        configSwitch(BIT7_TRIG_GATE_PARAM, 0.0f, 1.0f, 1.0f, "Bit-7 mode", { "Trigger", "Gate" });
        configOutput(BIT8_OUTPUT, "Bit-8");
        configSwitch(BIT8_TRIG_GATE_PARAM, 0.0f, 1.0f, 1.0f, "Bit-8 mode", { "Trigger", "Gate" });

        // Anded bit outputs
        configOutput(BIT12_OUTPUT, "Bit-1 AND -2");
        configSwitch(BIT12_TRIG_GATE_PARAM, 0.0f, 1.0f, 1.0f, "Bit-1 AND -2 mode", { "Trigger", "Gate" });
        configOutput(BIT13_OUTPUT, "Bit-1 AND -3");
        configSwitch(BIT13_TRIG_GATE_PARAM, 0.0f, 1.0f, 1.0f, "Bit-1 AND -3 mode", { "Trigger", "Gate" });
        configOutput(BIT14_OUTPUT, "Bit-1 AND -4");
        configSwitch(BIT14_TRIG_GATE_PARAM, 0.0f, 1.0f, 1.0f, "Bit-1 AND -4 mode", { "Trigger", "Gate" });
        configOutput(BIT15_OUTPUT, "Bit-1 AND -5");
        configSwitch(BIT15_TRIG_GATE_PARAM, 0.0f, 1.0f, 1.0f, "Bit-1 AND -5 mode", { "Trigger", "Gate" });
        configOutput(BIT23_OUTPUT, "Bit-2 AND -3");
        configSwitch(BIT23_TRIG_GATE_PARAM, 0.0f, 1.0f, 1.0f, "Bit-2 AND -3 mode", { "Trigger", "Gate" });
        configOutput(BIT24_OUTPUT, "Bit-2 AND -4");
        configSwitch(BIT24_TRIG_GATE_PARAM, 0.0f, 1.0f, 1.0f, "Bit-2 AND -4 mode", { "Trigger", "Gate" });
        configOutput(BIT34_OUTPUT, "Bit-3 AND -4");
        configSwitch(BIT34_TRIG_GATE_PARAM, 0.0f, 1.0f, 1.0f, "Bit-3 AND -4 mode", { "Trigger", "Gate" });
        configOutput(BIT47_OUTPUT, "Bit-4 AND -7");
        configSwitch(BIT47_TRIG_GATE_PARAM, 0.0f, 1.0f, 1.0f, "Bit-4 AND -7 mode", { "Trigger", "Gate" });

        // Pulse outputs (all 16 pulses as polyphonic gates)
        configOutput(PULSES_OUTPUT, "All 16 pulses gates");

        uint32_t inUsePattern = 0x00; // Bits in use pattern based on length
        const uint32_t allOnes = 0xFFFFFFFF;
        for (int i = 0; i < 32; i++) {
            inUsePattern = (inUsePattern << 1) | 1;
            inUseBits[i] = inUsePattern;
            notInUseBits[i] = allOnes ^ inUseBits[i];
        }

        resetBit32(0.f);
        setBit8(8);

        // Set InfNoise features (e.g. menu-items) 
        haveProcQuality = true;
		haveAutoProcQuality = false;
        haveOutQuantize = true;
        haveOutClipRange = true;
		haveGateDetect = true;
		haveGateHighLow = true;
		haveTrigDetect = true;
		haveTrigHighLow = true;
	}

    void resetBit32(float resetSwitchParam) { // 0=random, 1=all 1's, 2=all 0's
		bit32 = 0;
        for (int i = 0; i < 32; i++) {
            uint32_t bitValue = 0;
            if (resetSwitchParam < 0.5f)
                bitValue = (randomNorm() > 0.5f) ? 0x01 : 0x00;
            else if (resetSwitchParam > 1.5f)
                bitValue = 1;
		    bit32 |= (bitValue << i);
        }
	}

    inline void setBit8(int length) {
        if (length >= 8) {
			bit8 = bit32 & 0xFF;
        }
        else {
            uint32_t bitLow;
            switch (length) {
                case 3:
                    bitLow = bit32 & 0x07;
					bit8 = bitLow | (bitLow << 3) | (bitLow << 6);
					break;
				case 4:
                    bitLow = bit32 & 0x0F;
                    bit8 = bitLow | (bitLow << 4);
					break;
                case 5:
                    bitLow = bit32 & 0x1F;
                    bit8 = bitLow | (bitLow << 5);
                    break;
                case 6:
                    bitLow = bit32 & 0x3F;
                    bit8 = bitLow | (bitLow << 6);
                    break;
                case 7:
                    bitLow = bit32 & 0x7F;
                    bit8 = bitLow | (bitLow << 7);
                    break;
            }
            bit8 &= 0xFF;
		}
	}

    void randomizeBit(int length) {
        int bitIdx = params[BIT_CHANGE_PARAM].getValue() < 0.5f  // Bit to change
            ? 0
            : RandomUint32() % std::max(length, 8);

        uint32_t bitMask = 0x01 << bitIdx;
        uint32_t newBit = (bit32 & bitMask) >> bitIdx;

        float bitProb = params[BIT_PROB_PARAM].getValue();
        float absBitProb = fabs(bitProb);
        if ((absBitProb < 0.9999f) && (absBitProb < 0.0001f || absBitProb < randomNorm())) {   
            newBit = newBit == 0x01 ? 0x00 : 0x01; // Invert bit
        }
        else {  // Set bit to 0 or 1
            newBit = bitProb < 0.f ? 0 : 1;
        }

        bit32 = (bit32 & ~bitMask) | (newBit << bitIdx);
    }

    inline void rotateBits(int length) {
        uint32_t inUsePattern = bit32 & inUseBits[length-1];
        uint32_t lowBit = inUsePattern & 0x01;       
        inUsePattern = ((inUsePattern >> 1) | (lowBit << (length-1))) & inUseBits[length-1];
        bit32 = (bit32 & notInUseBits[length-1]) | inUsePattern;
	}

    inline int getLength() {
        int length = (int)params[LENGTH_PARAM].getValue();
        if (inputs[LENGTH_INPUT].isConnected()) {
            float addLength = (inputs[LENGTH_INPUT].getVoltage() / 10.f * params[LENGTH_TRIM_PARAM].getValue()) * 29.f;
            length += std::floor(addLength + 0.5f);
            length = clamp(length, 3, 32);
        }
        return length;
    }

    void onReset(const ResetEvent& e) override {
        InfNoiseModule::onReset(e);

        resetBit32(0.f);
        setBit8(8);
        updateBit8Lights = true;
        prevLength = -1;

        locked.setBoth(false);
        lockMode.setBoth(lockModeType::lm_Change);
        lockPreventsReset.setBoth(false);

        minCntrMax.setBoth(minCntrMaxType::mcm_Center);
        lockTrigger.reset();
        minCntrMaxTrigger.reset();
        resetTrigger.reset();
        clockTrigger.reset();
        rotateTrigger.reset();
        for (int i = 0; i < 16; i++) {
            pulseInTrigger[i].reset();
			pulseOutTrigger[i].reset();
		}
    }

    void dataFromJson(json_t* rootJ) override {
        InfNoiseModule::dataFromJson(rootJ);
        
        resetBit32(0.f);
        uint32_t randomBit32 = bit32;
        bit32 = getJsonUint32(rootJ, "bit32", randomBit32);
        updateBit8Lights = true;
        prevLength = -1;

        locked.setBoth(getJsonInt(rootJ, "locked", 0) == 1);
        lockMode.setBoth((lockModeType)getJsonInt(rootJ, "lockMode", (int)lm_Both));
        lockPreventsReset.setBoth(getJsonInt(rootJ, "lockPreventsReset", 0) == 1);
        minCntrMax.setBoth((minCntrMaxType)getJsonInt(rootJ, "minCntrMax", (int)minCntrMaxType::mcm_Center));
    }

    void dataToJson(json_t* rootJ) override {
        json_object_set_new(rootJ, "bit32", json_integer(bit32));
        json_object_set_new(rootJ, "locked", json_integer(locked.req ? 1 : 0));
        json_object_set_new(rootJ, "lockMode", json_integer((int)lockMode.req));
        json_object_set_new(rootJ, "lockPreventsReset", json_integer(lockPreventsReset.req ? 1 : 0));
        json_object_set_new(rootJ, "minCntrMax", json_integer((int)minCntrMax.req));
    }

    void setPulseOutput(int pulseIdx, bool onPulse) {
        // Polyphonic pulses-gates
        float voltage = 0.f;
        if (outputs[PULSES_OUTPUT].isConnected()) {
            voltage = onPulse
                ? voltValues[gateOutHigh.act]
                : voltValues[gateOutLow.act];
            outputs[PULSES_OUTPUT].setVoltage(voltage, pulseIdx);
        }

        if (outputs[BIT1_OUTPUT + pulseIdx].isConnected()) {
            // Monophonic puls-Trigger
            if (params[BIT1_TRIG_GATE_PARAM + pulseIdx].getValue() < 0.5f) {
                bool outTrigProc = pulseOutTrigger[pulseIdx].process(procSampleTime);
                if (pulseInTrigger[pulseIdx].process(onPulse ? 10.f : 0.f, 0.1f, 1.f)) {
                    if (!outTrigProc && onPulse) {
                        pulseOutTrigger[pulseIdx].trigger();
                    }
                }

                voltage = pulseOutTrigger[pulseIdx].isHigh()
                    ? voltValues[trigOutHigh.act]
                    : voltValues[trigOutLow.act];
            }
            else  // Monophonic puls-Gate
            {
                voltage = onPulse
                    ? voltValues[gateOutHigh.act]
                    : voltValues[gateOutLow.act];
            }

            // Output
            outputs[BIT1_OUTPUT + pulseIdx].setVoltage(voltage);
        }
    }

    void processParams(const ProcessArgs& args) {
        preProcessParams(args);
        //--------------------

        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Warray-bounds"
        outputs[PULSES_OUTPUT].setChannels(16); // Will  generate warning without the pragmas
        #pragma GCC diagnostic pop

        // Update bit8 lights if applicable
        if (updateBit8Lights) {
            updateBit8Lights = false;
            int length = getLength();
            setBit8(length);
            for (int i = 0; i < 8; i++) {
                lights[BIT1_LIGHT + i].setBrightness((bit8 & (1 << i)) ? 1.f : 0.f);
            }
        }

        //update locked light if applicable
        if (locked.needsUpdate())
        {
            locked.updateActual();
            lights[LOCK_LIGHT].setBrightness(locked.act ? 1.f : 0.f);
        }
        lockMode.updateActual();
        lockPreventsReset.updateActual();

        // Update Min/Center/Max lights
        if (minCntrMax.needsUpdate()) {
			minCntrMax.updateActual();
			lights[MIN_LIGHT].setBrightness(minCntrMax.act == minCntrMaxType::mcm_Min ? 1.f : 0.f);
			lights[CNTR_LIGHT].setBrightness(minCntrMax.act == minCntrMaxType::mcm_Center ? 1.f : 0.f);
			lights[MAX_LIGHT].setBrightness(minCntrMax.act == minCntrMaxType::mcm_Max ? 1.f : 0.f);
		}

        // Ensure unused pulses are "cleared" (a cable might be connected later)
        havePulse18 = false;
        havePulse916 = false;
        for (int i = 0; i < 16; i++) {
            if (!outputs[BIT1_OUTPUT + i].isConnected()) {
                pulseInTrigger[i].reset();
                pulseOutTrigger[i].reset();
                outputs[BIT1_OUTPUT + i].setVoltage(0.f);
            }
            else
            {
                if (params[BIT1_TRIG_GATE_PARAM + i].getValue() > 0.5f) {
                    pulseOutTrigger[i].reset(); // Set as Gate, so reset output-trigger
                }
                if (i < 8) havePulse18 = true; else havePulse916 = true;
            }
        }

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

        if (doProcess) {
            // Detect lock-input (sets "locked" variable)
            bool tmpLocked = locked.req;
            if (params[LOCK_BTN_PARAM].getValue() > 0.5f) { // Lock-button is pressed
                tmpLocked = true;
            }
            else if (inputs[LOCK_INPUT].isConnected()) {
                float lockInput = inputs[LOCK_INPUT].getVoltage();
                if (params[LOCK_SWITCH_PARAM].getValue() < 0.5f) {  // Lock-input is gate
                    tmpLocked = lockInput >= trueDetectValues[gateDetHigh.act];
                }
				else { // Lock-input is trigger
                    if (lockTrigger.process(lockInput,
                        trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act])) {
                        tmpLocked = !tmpLocked;
                    }
                }
            }
            else {  // Lock-button is not pressed and no lock-input
                tmpLocked = false;
            }
            if (locked.req != tmpLocked) {  // Ensure locked-lights are only updated when needed
                locked.setBoth(tmpLocked);
            }   

            // Detect reset-input (if not locked for reset)
            bool reset = false;
            if (!locked.act || !lockPreventsReset.act) {
                float resetInput = params[RESET_BTN_PARAM].getValue() > 0.5f
                    ? 10.f
                    : (inputs[RESET_INPUT].isConnected())
                        ? inputs[RESET_INPUT].getVoltage()
                        : 0.f;
                if (resetTrigger.process(resetInput,
                    trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act])) {
                    reset = true;
                }
            }
            // Handle reset (if applicable)
            if (reset) {
				resetBit32(params[RESET_SWITCH_PARAM].getValue());
				updateBit8Lights = true;
			}

            // Detect clock-input
            bool clock = false;
            float clockInput = params[CLOCK_BTN_PARAM].getValue() > 0.5f
                ? 10.f
                : (inputs[CLOCK_INPUT].isConnected()) 
                    ? inputs[CLOCK_INPUT].getVoltage() 
                    : 0.f;
            if (clockTrigger.process(clockInput,
                trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act])) {
                clock = true;
            }

            // Detect rotate-input
            bool rotate = false;
            float rotateInput = params[ROTATE_BTN_PARAM].getValue() > 0.5f
                ? 10.f
                : (inputs[ROTATE_INPUT].isConnected())
                    ? inputs[ROTATE_INPUT].getVoltage()
                    : clockInput;
            if (rotateTrigger.process(rotateInput,
                trueDetectValues[trigDetLow.act], trueDetectValues[trigDetHigh.act])) {
                rotate = true;
            }

            // Check if length has changed
            int length = getLength();
            if (length != prevLength) {
				updateBit8Lights = true;
                prevLength = length;
			}

            // Handle rotation (if not locked for rotation)
            bool lockedForRotate = locked.req && lockMode.req != lm_Change;
            if (rotate && !lockedForRotate) {
                rotateBits(length);
                updateBit8Lights = true;
			}

            // Handle bit-change (if not locked for bit-change)
            bool lockedForChange = locked.req && lockMode.req != lm_Rotate;
            if (clock && !lockedForChange) {
                float changeProb = params[CHANGE_PROB_PARAM].getValue();
                if (inputs[CHANGE_PROB_INPUT].isConnected())
                {
                    changeProb += inputs[CHANGE_PROB_INPUT].getVoltage() / 10.f * params[CHANGE_PROB_TRIM_PARAM].getValue();
                    changeProb = clamp(changeProb, 0.f, 1.f);
                }
                if (changeProb > 0.f && ((changeProb == 1.f) || (randomNorm() <= changeProb))) {
					randomizeBit(length);
					updateBit8Lights = true;
				}
			}

            // Handle Min/Center/Max-button (switch between Min, Center and Max)
            // Button idles at 1 (center), so map to 0-based before edge detection.
            if (minCntrMaxTrigger.process(params[MINCNTRMAX_BTN_PARAM].getValue(), 0.1f, 0.9f)) {
                minCntrMax.setBoth(minCntrMax.act == mcm_Max ? mcm_Min : static_cast<minCntrMaxType>(minCntrMax.act + 1));
            }

            // Set minValue and maxValue
            float rangeVolt = params[RANGE_PARAM].getValue();
            if (inputs[RANGE_INPUT].isConnected())
                rangeVolt += params[RANGE_TRIM_PARAM].getValue() * inputs[RANGE_INPUT].getVoltage();
            float minCntrMaxVolt = params[MINCNTRMAX_PARAM].getValue();
            if (inputs[MINCNTRMAX_INPUT].isConnected())
                minCntrMaxVolt += params[MINCNTRMAX_TRIM_PARAM].getValue() * inputs[MINCNTRMAX_INPUT].getVoltage();

            // Set minValue and maxValue
            float minValue;
            if (minCntrMax.act == mcm_Min) {
                minValue = minCntrMaxVolt;
            }
            else if (minCntrMax.act == mcm_Center) {
                float halfRange = rangeVolt / 2.f;
                minValue = minCntrMaxVolt - halfRange;
            }
            else { // mcm_Max
                minValue = minCntrMaxVolt - rangeVolt;
            }

            // Set Value output
            setBit8(length);
            float deltaVolt = (bit8 / 255.f) * rangeVolt;
            float voltage = minValue + deltaVolt;
            voltage = quantizeToMode(voltage, outQuantize.act);
            voltage = clipToVoltRange(voltage, outClipRange.act);
            outputs[BINARY_OUTPUT].setVoltage(voltage);

            // Set Inv output
            if (outputs[FIXED_OUTPUT].isConnected()) {
                const float oneEighth = 1.f / 8.f;
                int tempBit8 = bit8;
                float fixedVolt = 0.f;
                for (int i = 0; i < 8; i++) {
                    fixedVolt += ((tempBit8 & 1) * oneEighth);
                    tempBit8 >>= 1;
                }
                float voltage = minValue + fixedVolt * rangeVolt;
                voltage = quantizeToMode(voltage, outQuantize.act);
                voltage = clipToVoltRange(voltage, outClipRange.act);
                outputs[FIXED_OUTPUT].setVoltage(voltage);
            }

            // Set pulse outputs 1- 8 (single bit)
            bool havePulsesOutput = outputs[PULSES_OUTPUT].isConnected();
            if (havePulse18 || havePulsesOutput)
            {
                for (int i = 0; i < 8; i++) {
                    setPulseOutput(i, (bit8 & (1 << i)));
                }
            }

            // Set pulse outputs 9 - 16 (2 AND'ed bits)
            if (havePulse916 || havePulsesOutput)
            {
                setPulseOutput(8, ((bit8 & 0x03) == 0x03));  // 00000011
                setPulseOutput(9, ((bit8 & 0x05) == 0x05));  // 00000101
                setPulseOutput(10, ((bit8 & 0x09) == 0x09)); // 00001001
                setPulseOutput(11, ((bit8 & 0x11) == 0x11)); // 00010001
                setPulseOutput(12, ((bit8 & 0x06) == 0x06)); // 00000110
                setPulseOutput(13, ((bit8 & 0x0A) == 0x0A)); // 00001010
                setPulseOutput(14, ((bit8 & 0x0C) == 0x0C)); // 00001100
                setPulseOutput(15, ((bit8 & 0x48) == 0x48)); // 01001000
            }
        }

        cycle256++;
    }
};

struct TuringMachineModuleWidget : InfNoiseModuleWidget {
    infNoiseSmallButton<bc_green, true>* lockBtn;

    TuringMachineModuleWidget(TuringMachineModule *module) {
        initializeWidget(module, "res/TuringMachine");

        float clm = 84.391f;
        for (int i = 0; i < 8; i++) {
			addChild(createLightCentered<SmallLight<GreenLight>>(Vec(clm, 54.445f), module, TuringMachineModule::BIT1_LIGHT + i));
			clm -= 10.0737f;
		}
        addParam(createParamCentered<CKSS>(Vec(97.180f, 49.087f), module, TuringMachineModule::BIT_CHANGE_PARAM));

        const float knobColumn = 14.386f;
        const float trimColumn = 44.500f;
        const float inputColumn = 74.500f;
        const float bit1Column = 104.499f;
        const float bit2Column = 134.499f;
        const float controlRowSpacing = 35.258f;

        float controlRow = 87.009f;
        lockBtn = createParamCentered<infNoiseSmallButton<bc_green, true>>(Vec(knobColumn, controlRow), module, TuringMachineModule::LOCK_BTN_PARAM);
        addParam(lockBtn);
        addParam(createParamCentered<infNoiseLtSmallButton<bc_red>>(Vec(27.484f, 75.420f), module, TuringMachineModule::LOCK_BTN_LATCH_PARAM));
        addParam(createParamCentered<CKSS>(Vec(39.361f, controlRow), module, TuringMachineModule::LOCK_SWITCH_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(inputColumn, controlRow), module, TuringMachineModule::LOCK_INPUT));
        addChild(createLightCentered<SmallLight<RedLight>>(Vec(57.536f, 71.924f), module, TuringMachineModule::LOCK_LIGHT));

        controlRow += controlRowSpacing;
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(knobColumn, controlRow), module, TuringMachineModule::CHANGE_PROB_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(trimColumn, controlRow), module, TuringMachineModule::CHANGE_PROB_TRIM_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(inputColumn, controlRow), module, TuringMachineModule::CHANGE_PROB_INPUT));

        controlRow += controlRowSpacing;
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(knobColumn, controlRow), module, TuringMachineModule::BIT_PROB_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(trimColumn, controlRow), module, TuringMachineModule::BIT_PROB_TRIM_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(inputColumn, controlRow), module, TuringMachineModule::BIT_PROB_INPUT));
        
        controlRow += controlRowSpacing;
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(knobColumn, controlRow), module, TuringMachineModule::LENGTH_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(trimColumn, controlRow), module, TuringMachineModule::LENGTH_TRIM_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(inputColumn, controlRow), module, TuringMachineModule::LENGTH_INPUT));

        controlRow += controlRowSpacing;
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(knobColumn, controlRow), module, TuringMachineModule::RANGE_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(trimColumn, controlRow), module, TuringMachineModule::RANGE_TRIM_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(inputColumn, controlRow), module, TuringMachineModule::RANGE_INPUT));

        controlRow += controlRowSpacing;
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(knobColumn, controlRow), module, TuringMachineModule::MINCNTRMAX_PARAM));
        addParam(createParamCentered<Trimpot>(Vec(trimColumn, controlRow), module, TuringMachineModule::MINCNTRMAX_TRIM_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(inputColumn, controlRow), module, TuringMachineModule::MINCNTRMAX_INPUT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(27.484f, 243.237), module, TuringMachineModule::MIN_LIGHT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(45.640f, 243.237), module, TuringMachineModule::CNTR_LIGHT));
        addChild(createLightCentered<TinyLight<GreenLight>>(Vec(63.615f, 243.237), module, TuringMachineModule::MAX_LIGHT));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green, true>>(Vec(74.499f, 244.295f), module, TuringMachineModule::MINCNTRMAX_BTN_PARAM));
        //addParam(createParamCentered<infNoiseLtSmallButton<bc_green, true>>(Vec(74.499f, 247.295f), module, TuringMachineModule::MINCNTRMAX_BTN_PARAM));

        controlRow += controlRowSpacing;
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green, true>>(Vec(26.458f, 287.759f), module, TuringMachineModule::RESET_BTN_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(knobColumn, controlRow), module, TuringMachineModule::RESET_INPUT));
        addParam(createParamCentered<CKSSThree>(Vec(38.359f, controlRow), module, TuringMachineModule::RESET_SWITCH_PARAM));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(inputColumn, controlRow), module, TuringMachineModule::FIXED_OUTPUT));

        controlRow += controlRowSpacing;
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(knobColumn, controlRow), module, TuringMachineModule::CLOCK_INPUT));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green, true>>(Vec(26.458f, 323.214f), module, TuringMachineModule::CLOCK_BTN_PARAM));
        addInput(createInputCentered<ThemedPJ301MPort>(Vec(trimColumn, controlRow), module, TuringMachineModule::ROTATE_INPUT));
        addParam(createParamCentered<infNoiseLtSmallButton<bc_green, true>>(Vec(56.245f, 323.214f), module, TuringMachineModule::ROTATE_BTN_PARAM));
        addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(inputColumn, controlRow), module, TuringMachineModule::BINARY_OUTPUT));

        controlRow = 87.009f;
        float trigGateClmOfs = -9.071f;
        float trigGateRowOfs = -14.021f;
        controlRow = 87.009f;
        for (int i = 0; i < 8; i++) {
            addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(bit1Column, controlRow), module, TuringMachineModule::BIT1_OUTPUT + i));
            addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_red, bc_green>>(
                Vec(bit1Column + trigGateClmOfs, controlRow + trigGateRowOfs), module, TuringMachineModule::BIT1_TRIG_GATE_PARAM + i));

            addOutput(createOutputCentered<ThemedPJ301MPort>(Vec(bit2Column, controlRow), module, TuringMachineModule::BIT12_OUTPUT + i));
            addParam(createParamCentered<infNoiseLtSmallButtonSwitch<bc_red, bc_green>>(
                Vec(bit2Column + trigGateClmOfs, controlRow + trigGateRowOfs), module, TuringMachineModule::BIT12_TRIG_GATE_PARAM + i));
            
            controlRow += controlRowSpacing;
        }

        addOutput(createOutputCentered<infNoiseThemedPolyPort>(Vec(bit2Column, 54.150), module, TuringMachineModule::PULSES_OUTPUT));
    }

    void step() override {
        if (module) {
            applyButtonMomentary(lockBtn, module->params[TuringMachineModule::LOCK_BTN_LATCH_PARAM].getValue() < 0.5f);
        }

        InfNoiseModuleWidget::step();
    }

    void appendContextMenu(Menu* menu) override {
        InfNoiseModuleWidget::appendContextMenu(menu);
        TuringMachineModule* module = dynamic_cast<TuringMachineModule*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator);

		menu->addChild(createIndexPtrSubmenuItem("Lock-mode",
		 	{ "Both change and rotate", "Only bit-change", "Only rotate" },
		 	&module->lockMode.req
        ));
        
        menu->addChild(createBoolPtrMenuItem("Lock prevents reset", "", &module->lockPreventsReset.req));

        // Appends proc-qual. and clip-range menus
        appendInfNoiseMenuItems(menu);
    }
};

Model *modelTuringMachine = createModel<TuringMachineModule, TuringMachineModuleWidget>("TuringMachine");
