// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#pragma once
#include <array>
#include <cmath>
#include <rack.hpp>
using namespace ::rack;

// Top-level in-header, can't include other in-headers here


//-----------------------------------------------------------------------------
// Common Json get functions
//-----------------------------------------------------------------------------
/// @brief Get a boolean value from a JSON object using the specified key.
/// If key is not found, the default value is returned.
/// @param rootJ JSON object to get value from.
/// @param key Key to get value from.
/// @param defaultValue Default value to return if key is not found.
inline bool getJsonBool(json_t* rootJ, const char* key, const bool defaultValue = false) {
	json_t* jsonObj = json_object_get(rootJ, key);
	return (jsonObj)
		? json_boolean_value(jsonObj)
		: defaultValue;
}

/// @brief Get an int value from a JSON object using the specified key.
/// If key is not found, the default value is returned.
/// @param rootJ JSON object to get value from.
/// @param key Key to get value from.
/// @param defaultValue Default value to return if key is not found.
inline int getJsonInt(json_t* rootJ, const char* key, const int defaultValue = 0) {
	json_t* jsonObj = json_object_get(rootJ, key);
	return (jsonObj)
		? json_integer_value(jsonObj)
		: defaultValue;
}

/// @brief Get an uint32_t from a JSON object using the specified key.
/// If key is not found, the default value is returned.
/// @param rootJ JSON object to get value from.
/// @param key Key to get value from.
/// @param defaultValue Default value to return if key is not found.
inline uint32_t getJsonUint32(json_t* rootJ, const char* key, const uint32_t defaultValue = 0) {
	json_t* jsonObj = json_object_get(rootJ, key);
	return (jsonObj)
		? json_integer_value(jsonObj)
		: defaultValue;
}

/// @brief Get a float from a JSON object using the specified key.
/// If key is not found, the default value is returned.
/// @param rootJ JSON object to get value from.
/// @param key Key to get value from.
/// @param defaultValue Default value to return if key is not found.
inline float getJsonFloat(json_t* rootJ, const char* key, const float defaultValue = 0.f) {
	json_t* jsonObj = json_object_get(rootJ, key);
	return (jsonObj)
		? json_real_value(jsonObj)
		: defaultValue;
}

/// @brief Get a std::string from a JSON object using the specified key.
/// If key is not found, the default value is returned.
/// @param rootJ JSON object to get value from.
/// @param key Key to get value from.
/// @param defaultValue Default value to return if key is not found.
inline std::string getJsonString(json_t* rootJ, const char* key, const std::string& defaultValue = "") {
	json_t* jsonObj = json_object_get(rootJ, key);
	return (jsonObj)
		? json_string_value(jsonObj)
		: defaultValue;
}


//-----------------------------------------------------------------------------
// Common Json array get/set
//-----------------------------------------------------------------------------

// --- int ---
/// @brief Overwrite dst[] from a JSON integer array at key, when present and valid.
inline void overlayJsonIntArray(json_t* rootJ, const char* key, int* dst, size_t count) {
	json_t* arrJ = json_object_get(rootJ, key);
	if (!arrJ || !json_is_array(arrJ))
		return;
	size_t arrSize = json_array_size(arrJ);
	size_t lim = arrSize < count ? arrSize : count;
	for (size_t i = 0; i < lim; i++) {
		json_t* elemJ = json_array_get(arrJ, i);
		if (elemJ && json_is_integer(elemJ))
			dst[i] = (int)json_integer_value(elemJ);
	}
}

/// @brief Load int array from JSON. dst[] is filled with defaultValue, then valid array elements overwrite.
/// @param rootJ JSON object to get value from.
/// @param key Key of JSON array.
/// @param dst Destination buffer (count elements).
/// @param count Number of elements in dst.
/// @param defaultValue Value for every element when key is missing or not an array.
inline void getJsonIntArray(json_t* rootJ, const char* key, int* dst, size_t count, int defaultValue = 0) {
	for (size_t i = 0; i < count; i++)
		dst[i] = defaultValue;
	overlayJsonIntArray(rootJ, key, dst, count);
}

/// @brief Load int array from JSON. dst[] is filled from defaultValues[], then valid array elements overwrite.
/// @param rootJ JSON object to get value from.
/// @param key Key of JSON array.
/// @param dst Destination buffer (count elements).
/// @param count Number of elements in dst.
/// @param defaultValues Per-element defaults (count elements).
inline void getJsonIntArray(json_t* rootJ, const char* key, int* dst, size_t count, const int* defaultValues) {
	for (size_t i = 0; i < count; i++)
		dst[i] = defaultValues[i];
	overlayJsonIntArray(rootJ, key, dst, count);
}

/// @brief Store int array as a JSON array at key.
/// @param rootJ JSON object to set value on.
/// @param key Key for JSON array.
/// @param src Source buffer (count elements).
/// @param count Number of elements in src.
inline void setJsonIntArray(json_t* rootJ, const char* key, const int* src, size_t count) {
	json_t* arrJ = json_array();
	for (size_t i = 0; i < count; i++)
		json_array_append_new(arrJ, json_integer(src[i]));
	json_object_set_new(rootJ, key, arrJ);
}

// --- bool ---
inline void overlayJsonBoolArray(json_t* rootJ, const char* key, bool* dst, size_t count) {
	json_t* arrJ = json_object_get(rootJ, key);
	if (!arrJ || !json_is_array(arrJ))
		return;
	size_t arrSize = json_array_size(arrJ);
	size_t lim = arrSize < count ? arrSize : count;
	for (size_t i = 0; i < lim; i++) {
		json_t* elemJ = json_array_get(arrJ, i);
		if (elemJ && json_is_boolean(elemJ))
			dst[i] = json_boolean_value(elemJ);
	}
}

inline void getJsonBoolArray(json_t* rootJ, const char* key, bool* dst, size_t count, bool defaultValue = false) {
	for (size_t i = 0; i < count; i++)
		dst[i] = defaultValue;
	overlayJsonBoolArray(rootJ, key, dst, count);
}

inline void getJsonBoolArray(json_t* rootJ, const char* key, bool* dst, size_t count, const bool* defaultValues) {
	for (size_t i = 0; i < count; i++)
		dst[i] = defaultValues[i];
	overlayJsonBoolArray(rootJ, key, dst, count);
}

inline void setJsonBoolArray(json_t* rootJ, const char* key, const bool* src, size_t count) {
	json_t* arrJ = json_array();
	for (size_t i = 0; i < count; i++)
		json_array_append_new(arrJ, json_boolean(src[i]));
	json_object_set_new(rootJ, key, arrJ);
}

// --- uint32_t ---
inline void overlayJsonUint32Array(json_t* rootJ, const char* key, uint32_t* dst, size_t count) {
	json_t* arrJ = json_object_get(rootJ, key);
	if (!arrJ || !json_is_array(arrJ))
		return;
	size_t arrSize = json_array_size(arrJ);
	size_t lim = arrSize < count ? arrSize : count;
	for (size_t i = 0; i < lim; i++) {
		json_t* elemJ = json_array_get(arrJ, i);
		if (elemJ && json_is_integer(elemJ))
			dst[i] = (uint32_t)json_integer_value(elemJ);
	}
}

inline void getJsonUint32Array(json_t* rootJ, const char* key, uint32_t* dst, size_t count, uint32_t defaultValue = 0) {
	for (size_t i = 0; i < count; i++)
		dst[i] = defaultValue;
	overlayJsonUint32Array(rootJ, key, dst, count);
}

inline void getJsonUint32Array(json_t* rootJ, const char* key, uint32_t* dst, size_t count, const uint32_t* defaultValues) {
	for (size_t i = 0; i < count; i++)
		dst[i] = defaultValues[i];
	overlayJsonUint32Array(rootJ, key, dst, count);
}

inline void setJsonUint32Array(json_t* rootJ, const char* key, const uint32_t* src, size_t count) {
	json_t* arrJ = json_array();
	for (size_t i = 0; i < count; i++)
		json_array_append_new(arrJ, json_integer((json_int_t)src[i]));
	json_object_set_new(rootJ, key, arrJ);
}

// --- float ---
inline void overlayJsonFloatArray(json_t* rootJ, const char* key, float* dst, size_t count) {
	json_t* arrJ = json_object_get(rootJ, key);
	if (!arrJ || !json_is_array(arrJ))
		return;
	size_t arrSize = json_array_size(arrJ);
	size_t lim = arrSize < count ? arrSize : count;
	for (size_t i = 0; i < lim; i++) {
		json_t* elemJ = json_array_get(arrJ, i);
		if (elemJ && json_is_real(elemJ))
			dst[i] = (float)json_real_value(elemJ);
	}
}

inline void getJsonFloatArray(json_t* rootJ, const char* key, float* dst, size_t count, float defaultValue = 0.f) {
	for (size_t i = 0; i < count; i++)
		dst[i] = defaultValue;
	overlayJsonFloatArray(rootJ, key, dst, count);
}

inline void getJsonFloatArray(json_t* rootJ, const char* key, float* dst, size_t count, const float* defaultValues) {
	for (size_t i = 0; i < count; i++)
		dst[i] = defaultValues[i];
	overlayJsonFloatArray(rootJ, key, dst, count);
}

inline void setJsonFloatArray(json_t* rootJ, const char* key, const float* src, size_t count) {
	json_t* arrJ = json_array();
	for (size_t i = 0; i < count; i++)
		json_array_append_new(arrJ, json_real(src[i]));
	json_object_set_new(rootJ, key, arrJ);
}

// --- std::string ---
inline void overlayJsonStringArray(json_t* rootJ, const char* key, std::string* dst, size_t count) {
	json_t* arrJ = json_object_get(rootJ, key);
	if (!arrJ || !json_is_array(arrJ))
		return;
	size_t arrSize = json_array_size(arrJ);
	size_t lim = arrSize < count ? arrSize : count;
	for (size_t i = 0; i < lim; i++) {
		json_t* elemJ = json_array_get(arrJ, i);
		if (elemJ && json_is_string(elemJ))
			dst[i] = json_string_value(elemJ);
	}
}

inline void getJsonStringArray(json_t* rootJ, const char* key, std::string* dst, size_t count, const std::string& defaultValue = "") {
	for (size_t i = 0; i < count; i++)
		dst[i] = defaultValue;
	overlayJsonStringArray(rootJ, key, dst, count);
}

inline void getJsonStringArray(json_t* rootJ, const char* key, std::string* dst, size_t count, const std::string* defaultValues) {
	for (size_t i = 0; i < count; i++)
		dst[i] = defaultValues[i];
	overlayJsonStringArray(rootJ, key, dst, count);
}

inline void setJsonStringArray(json_t* rootJ, const char* key, const std::string* src, size_t count) {
	json_t* arrJ = json_array();
	for (size_t i = 0; i < count; i++)
		json_array_append_new(arrJ, json_string(src[i].c_str()));
	json_object_set_new(rootJ, key, arrJ);
}


//-----------------------------------------------------------------------------
// Actual/requested-value ("act" used by process, "req" set/read by menu-items)
//-----------------------------------------------------------------------------

template <typename T>
/// @brief Actual/requested-value ("act" used by process, "req" set/read by menu-items)
struct actReqValue {
	T act;  // Actual value used by process (e.g. each cycle)
	T req;  // E.g. Set/Read by menu-items (updates "act" each 256 cycles)
	bool mustUpdate = true;  // If true "needsUpdate" will return true even if (act == req) 

	/// @brief Sets "act" and "req" to sepecified value, and sets "mustUpdate" to true
	/// @param value that "req" and "act" should be set to.
	actReqValue(T value) {
		act = value;
		req = value;
		mustUpdate = true;
	}

	/// @brief Sets "act" to the value of "req", and sets "mustUpdate" to false
	/// called in processParams, to set act=req)
	inline void updateActual() {
		act = req;
		mustUpdate = false;
	}

	/// @brief Returns true if "act != req", or "mustUpdate" is true
	/// (when necessary, checked in processParams to detect changes)
	/// @return True if "act != req", or "mustUpdate" is true
	inline bool needsUpdate() {
		return (mustUpdate || act != req);
	}

	/// @brief Sets both "req" and "act" to the value, and mustUpdate defaults to true.
	/// Only call with setMustUpdate true, when "mustProcessParams" is/will be set to
	/// true (e.g. called from "onReset" or "dataFromJson").
	/// @param value that "req" and "act" should be set to.
	/// @param setMustUpdate mustUpdate will be set to this value (default to true)
	inline void setBoth(T value, bool setMustUpdate = true) {
		req = value;
		act = value;
		mustUpdate = setMustUpdate;
	}
};


//-----------------------------------------------------------------------------
// Default Volt-values (e.g. normalization-, min/max- or true/false-voltage)
//-----------------------------------------------------------------------------
// "m"="minus", "zero"="zero", "p"="plus", "_" decimal point
enum voltValue {
	v_m10, v_m8, v_m5, v_m2_5, v_m2, v_m1_5, v_m1, v_m_5, v_m_2, v_m_1,	v_zero, 
	v_p_1, v_p_2, v_p_5, v_p1, v_p1_5, v_p2, v_p2_5, v_p5, v_p8, v_p10
};
const int voltValueCount = voltValue::v_p10 + 1;
const voltValue v_GateHigh = v_p10; // Output-Gate High: +10V
const voltValue v_GateLow = v_zero; // Output-Gate Low: 0V
const voltValue v_TriggerHigh = v_p10; // Output-Trigger High: +10V
const voltValue v_TriggerLow = v_zero; // Output-Trigger Low: 0V
const float voltValues[]{ -10.f, -8.f, -5.f, -2.5f, -2.f, -1.5f, 
	-1.f, -0.5f, -0.2f, -0.1f, 0.f, 0.1f, 0.2f, 0.5f, 1.f, 1.5f, 
	2.f, 2.5f, 5.f, 8.f, 10.f };

inline std::string getVoltName(voltValue voltIdx) {
	if (voltIdx == v_m2_5)
		return "-2.5V";
	if (voltIdx == v_m1_5)
		return "-1.5V";
	if (voltIdx == v_m_5)
		return "-0.5V";
	if (voltIdx == v_m_2)
		return "-0.2V";
	if (voltIdx == v_m_1)
		return "-0.1V";
	if (voltIdx == v_p_1)
		return "0.1V";
	if (voltIdx == v_p_2)
		return "0.2V";
	if (voltIdx == v_p_5)
		return "0.5V";
	if (voltIdx == v_p1_5)
		return "1.5V";
	if (voltIdx == v_p2_5)
		return "2.5V";
	return std::to_string((int)voltValues[voltIdx]) + "V";
}

inline std::string getVoltShortName(voltValue voltIdx) {
	if (voltIdx == v_m2_5)
		return "-2.5";
	if (voltIdx == v_m1_5)
		return "-1.5";
	if (voltIdx == v_m_5)
		return "-0.5";
	if (voltIdx == v_m_2)
		return "-0.2";
	if (voltIdx == v_m_1)
		return "-0.1";
	if (voltIdx == v_p_1)
		return "0.1";
	if (voltIdx == v_p_2)
		return "0.2";
	if (voltIdx == v_p_5)
		return "0.5";
	if (voltIdx == v_p1_5)
		return "1.5";
	if (voltIdx == v_p2_5)
		return "2.5";
	return std::to_string((int)voltValues[voltIdx]);
}

inline std::vector<std::string> getVoltValuesNames() {
	std::vector<std::string> names;
	for (int i = 0; i < voltValueCount; i++)
		names.push_back(getVoltName((voltValue)i));

	return names;
}

inline std::vector<std::string> getVoltValuesShortNames() {
	std::vector<std::string> names;
	for (int i = 0; i < voltValueCount; i++)
		names.push_back(getVoltShortName((voltValue)i));

	return names;
}


//-----------------------------------------------------------------------------
// Rate Chaos (per-cycle randomization of an LFO/rate phase-step)
//-----------------------------------------------------------------------------
// 0% = off (factor always 1.0), 100% = factor in [0.1, 10] at a phase wrap.
enum rateChaos {
	rc_0, rc_5, rc_10, rc_15, rc_20, rc_25, rc_30, rc_35, rc_40, rc_45, rc_50,
	rc_55, rc_60, rc_65, rc_70, rc_75, rc_80, rc_85, rc_90, rc_95, rc_100
};
const int rateChaosCount = rateChaos::rc_100 + 1;
const rateChaos rc_default = rc_0;  // default: no chaos
const float rateChaosValues[]{ 0.f, 0.05f, 0.10f, 0.15f, 0.20f, 0.25f, 0.30f,
	0.35f, 0.40f, 0.45f, 0.50f, 0.55f, 0.60f, 0.65f, 0.70f, 0.75f, 0.80f,
	0.85f, 0.90f, 0.95f, 1.0f };
// Max possible chaos factor per setting (= 10^chaos); used to pick process-quality
// for the worst-case (fastest) cycle. rateChaosMaxFactor[rc_0] == 1.0 (no change).
const float rateChaosMaxFactor[]{ 1.0f, 1.1220f, 1.2589f, 1.4125f, 1.5849f,
	1.7783f, 1.9953f, 2.2387f, 2.5119f, 2.8184f, 3.1623f, 3.5481f, 3.9811f,
	4.4668f, 5.0119f, 5.6234f, 6.3096f, 7.0795f, 7.9433f, 8.9125f, 10.0f };

inline std::vector<std::string> getRateChaosNames() {
	std::vector<std::string> names;
	for (int i = 0; i < rateChaosCount; i++)
		names.push_back(i == 0 ? "0% (default)"
			: string::f("%d%% (%.3f)", i * 5, rateChaosMaxFactor[i]));

	return names;
}


//-----------------------------------------------------------------------------
// Volt interval-values (note intervals in 1/12 V steps: -11 to +11 semitones)
//-----------------------------------------------------------------------------
enum voltIntervalValue {
	v_i_m11, v_i_m10, v_i_m9, v_i_m8, v_i_m7, v_i_m6, v_i_m5, v_i_m4, v_i_m3, v_i_m2, v_i_m1,
	v_i_zero,
	v_i_p1, v_i_p2, v_i_p3, v_i_p4, v_i_p5, v_i_p6, v_i_p7, v_i_p8, v_i_p9, v_i_p10, v_i_p11
};
const int voltIntervalValueCount = voltIntervalValue::v_i_p11 + 1;
const float voltIntervalValues[]{
	-11.f / 12.f, -10.f / 12.f, -9.f / 12.f, -8.f / 12.f, -7.f / 12.f, -6.f / 12.f,
	-5.f / 12.f, -4.f / 12.f, -3.f / 12.f, -2.f / 12.f, -1.f / 12.f,
	0.f,
	1.f / 12.f, 2.f / 12.f, 3.f / 12.f, 4.f / 12.f, 5.f / 12.f, 6.f / 12.f,
	7.f / 12.f, 8.f / 12.f, 9.f / 12.f, 10.f / 12.f, 11.f / 12.f
};

inline std::string getVoltIntervalName(voltIntervalValue intervalIdx) {
	float v = voltIntervalValues[intervalIdx];
	int n = (int)intervalIdx - 11;  // v_i_m11..v_i_zero..v_i_p11 -> -11..0..+11
	char buf[32];
	snprintf(buf, sizeof(buf), "%.4f", v);
	std::string s(buf);
	if (n != 0)
		s += " (" + std::to_string(n > 0 ? n : -n) + "/12)";
	return s;
}

inline std::string getVoltIntervalShortName(voltIntervalValue intervalIdx) {
	float v = voltIntervalValues[intervalIdx];
	char buf[32];
	snprintf(buf, sizeof(buf), "%.4f", v);
	return std::string(buf);
}

inline std::vector<std::string> getVoltIntervalValuesNames() {
	std::vector<std::string> names;
	for (int i = 0; i < voltIntervalValueCount; i++)
		names.push_back(getVoltIntervalName((voltIntervalValue)i));
	return names;
}

inline std::vector<std::string> getVoltIntervalValuesShortNames() {
	std::vector<std::string> names;
	for (int i = 0; i < voltIntervalValueCount; i++)
		names.push_back(getVoltIntervalShortName((voltIntervalValue)i));
	return names;
}

//-----------------------------------------------------------------------------
// Note interval sets (8 semitones, typically used for "set multiple knobs")
//-----------------------------------------------------------------------------
struct noteIntervalSet8 {
	const char* name;
	int noteCount;
	std::array<int, 8> semitones;
};

// Common scales: first 8 ascending notes (includes octave where meaningful)
const std::array<noteIntervalSet8, 8> scaleIntervalSets8{{
	{ "Major (Ionian) (8 notes)", 8, { 0, 2, 4, 5, 7, 9, 11, 12 } },
	{ "Natural minor (Aeolian) (8 notes)", 8, { 0, 2, 3, 5, 7, 8, 10, 12 } },
	{ "Dorian (8 notes)", 8, { 0, 2, 3, 5, 7, 9, 10, 12 } },
	{ "Mixolydian (8 notes)", 8, { 0, 2, 4, 5, 7, 9, 10, 12 } },
	{ "Harmonic minor (8 notes)", 8, { 0, 2, 3, 5, 7, 8, 11, 12 } },
	{ "Melodic minor (8 notes)", 8, { 0, 2, 3, 5, 7, 9, 11, 12 } },
	{ "Pentatonic major (5 notes)", 5, { 0, 2, 4, 7, 9, 12, 14, 16 } },
	{ "Pentatonic minor (5 notes)", 5, { 0, 3, 5, 7, 10, 12, 15, 17 } },
}};

// Common chords: tones + extensions/octave repeats to fill 8 entries
const std::array<noteIntervalSet8, 10> chordIntervalSets8{{
	{ "Major triad (3 notes)", 3, { 0, 4, 7, 12, 16, 19, 24, 28 } },
	{ "Minor triad (3 notes)", 3, { 0, 3, 7, 12, 15, 19, 24, 27 } },
	{ "Diminished triad (3 notes)", 3, { 0, 3, 6, 12, 15, 18, 24, 27 } },
	{ "Augmented triad (3 notes)", 3, { 0, 4, 8, 12, 16, 20, 24, 28 } },
	{ "Sus2 (3 notes)", 3, { 0, 2, 7, 12, 14, 19, 24, 26 } },
	{ "Sus4 (3 notes)", 3, { 0, 5, 7, 12, 17, 19, 24, 29 } },
	{ "Major 7 (4 notes)", 4, { 0, 4, 7, 11, 12, 16, 19, 23 } },
	{ "Minor 7 (4 notes)", 4, { 0, 3, 7, 10, 12, 15, 19, 22 } },
	{ "Dominant 7 (4 notes)", 4, { 0, 4, 7, 10, 12, 16, 19, 22 } },
	{ "Half-diminished 7 (4 notes)", 4, { 0, 3, 6, 10, 12, 15, 18, 22 } },
}};

/// @brief Applies note intervals from a set to successive params (knobs).
/// Each semitone is converted to volts by dividing by 12 (1V/oct).
/// @param module Module owning params to set.
/// @param firstParamId First param index in a consecutive range.
/// @param knobCount Number of successive knobs to set (clamped to 0..8).
/// @param set Interval set to apply.
/// @param fillBeyondNoteCount If true uses up to 8 entries; otherwise uses
/// at most noteCount entries from the set.
inline void applyNoteIntervalSetToParams(Module* module, int firstParamId,
	int knobCount, const noteIntervalSet8& set, bool fillBeyondNoteCount = true) {
	if (!module || knobCount <= 0)
		return;

	int maxNotes = fillBeyondNoteCount ? 8 : set.noteCount;
	if (maxNotes < 0)
		maxNotes = 0;
	else if (maxNotes > 8)
		maxNotes = 8;

	int applyCount = knobCount;
	if (applyCount < 0)
		applyCount = 0;
	else if (applyCount > 8)
		applyCount = 8;
	if (applyCount > maxNotes)
		applyCount = maxNotes;

	for (int i = 0; i < applyCount; i++)
		module->params[firstParamId + i].setValue((float)set.semitones[i] / 12.f);
}

/// @brief Applies a scale interval set (selected by index) to successive params.
inline void applyScaleIntervalSetToParams(Module* module, int firstParamId,
	int knobCount, int scaleIdx, bool fillBeyondNoteCount = true) {
	if (scaleIdx < 0 || scaleIdx >= (int)scaleIntervalSets8.size())
		return;
	applyNoteIntervalSetToParams(module, firstParamId, knobCount,
		scaleIntervalSets8[scaleIdx], fillBeyondNoteCount);
}

/// @brief Applies a chord interval set (selected by index) to successive params.
inline void applyChordIntervalSetToParams(Module* module, int firstParamId,
	int knobCount, int chordIdx, bool fillBeyondNoteCount = true) {
	if (chordIdx < 0 || chordIdx >= (int)chordIntervalSets8.size())
		return;
	applyNoteIntervalSetToParams(module, firstParamId, knobCount,
		chordIntervalSets8[chordIdx], fillBeyondNoteCount);
}

/// @brief Gets all scale-set names for menu items.
inline std::vector<std::string> getScaleIntervalSetNames() {
	std::vector<std::string> names;
	for (int i = 0; i < (int)scaleIntervalSets8.size(); i++)
		names.push_back(scaleIntervalSets8[i].name);
	return names;
}

/// @brief Gets all chord-set names for menu items.
inline std::vector<std::string> getChordIntervalSetNames() {
	std::vector<std::string> names;
	for (int i = 0; i < (int)chordIntervalSets8.size(); i++)
		names.push_back(chordIntervalSets8[i].name);
	return names;
}

/// @brief Adds "Set as scale" with all scale presets as submenu items.
inline void appendScaleSetMenuItems(Menu* menu, Module* module, int firstParamId,
	int knobCount, bool fillBeyondNoteCount = true,
	const std::string& title = "Set as scale") {
	if (!menu || !module)
		return;

	menu->addChild(createSubmenuItem(title, "", [=](Menu* scaleMenu) {
		for (int i = 0; i < (int)scaleIntervalSets8.size(); i++) {
			scaleMenu->addChild(createMenuItem(scaleIntervalSets8[i].name, "", [=]() {
				applyScaleIntervalSetToParams(module, firstParamId, knobCount, i, fillBeyondNoteCount);
			}));
		}
	}));
}

/// @brief Adds "Set as chord" with all chord presets as submenu items.
inline void appendChordSetMenuItems(Menu* menu, Module* module, int firstParamId,
	int knobCount, bool fillBeyondNoteCount = true,
	const std::string& title = "Set as chord") {
	if (!menu || !module)
		return;

	menu->addChild(createSubmenuItem(title, "", [=](Menu* chordMenu) {
		for (int i = 0; i < (int)chordIntervalSets8.size(); i++) {
			chordMenu->addChild(createMenuItem(chordIntervalSets8[i].name, "", [=]() {
				applyChordIntervalSetToParams(module, firstParamId, knobCount, i, fillBeyondNoteCount);
			}));
		}
	}));
}

/// @brief Adds both top-level set menus: "Set as scale" and "Set as chord".
inline void appendScaleChordSetMenuItems(Menu* menu, Module* module, int firstParamId,
	int knobCount, bool fillBeyondNoteCount = true) {
	appendScaleSetMenuItems(menu, module, firstParamId, knobCount, fillBeyondNoteCount, "Set as scale");
	appendChordSetMenuItems(menu, module, firstParamId, knobCount, fillBeyondNoteCount, "Set as chord");
}


//-----------------------------------------------------------------------------
// Volt tolerance-values (when two values are "considered identical")
//-----------------------------------------------------------------------------
enum voltTolValue {
	vt_zero, vt_0_0001, vt_0_001, vt_0_01, vt_hnt, vt_nt, v_0_1, vt_2nt, 
	vt_3nt,	vt_4nt, vt_6nt, vt_1, vt_2, vt_2_5, vt_3_33, vt_5
};
const int voltTolValueCount = voltTolValue::vt_5 + 1;
const float voltTolValues[]{ 0.00001f, 0.0001f, 0.001f, 0.01f, 1.f / 24.f, 
	1.f / 12.f, 0.1f, 2.f / 12.f, 3.f / 12.f, 4.f / 12.f, 6.f / 12.f, 1.f, 
	2.f, 2.5f, 10.f / 3.f, 5.f };

inline std::vector<std::string> getVoltTolValuesNames() {
	std::vector<std::string> names;
	names.push_back("0.00001");
	names.push_back("0.0001");
	names.push_back("0.001");
	names.push_back("0.01");
	names.push_back("0.042 (half note = 1/24)");
	names.push_back("0.083 (note = 1/12)");
	names.push_back("0.1");
	names.push_back("0.166 (2/12 = 2 notes)");
	names.push_back("0.25 (3/12 = 3 notes)");
	names.push_back("0.333 (4/12 = 4 notes)");
	names.push_back("0.5 (6/12 = 6 notes)");
	names.push_back("1.0 (1 octave)");
	names.push_back("2.0 (10V / 5)");
	names.push_back("2.5 (10V / 4)");
	names.push_back("3.333 (10V / 3)");
	names.push_back("5.0 (10V / 2)");

	return names;
}

inline std::vector<std::string> getVoltTolValuesShortNames() {
	std::vector<std::string> names;
	names.push_back("0.00001");
	names.push_back("0.0001");
	names.push_back("0.001");
	names.push_back("0.01");
	names.push_back("0.042");
	names.push_back("0.083");
	names.push_back("0.1");
	names.push_back("0.166");
	names.push_back("0.25");
	names.push_back("0.333");
	names.push_back("0.5");
	names.push_back("1.0");
	names.push_back("2.0");
	names.push_back("2.5");
	names.push_back("3.333");
	names.push_back("5.0");

	return names;
}


//-----------------------------------------------------------------------------
// True detect-values (used to detect when a volt-value is considered true/on)
// - Also contains the "false-detect" names (e.g. as a low detect for gate/trigger)
//-----------------------------------------------------------------------------
// "gt"="greater-than", "ge"="greater-or-equal", "_"="0." (e.g. "ge_1" is ">=0.1")
enum trueDetectValue {
	td_gt0, td_ge_1, td_ge_2, td_ge_5, td_ge1, td_ge1_5, td_ge2,
	td_ge2_5, td_ge3, td_ge4, td_ge5, td_ge8, td_ge10
};
const int trueDetectValueCount = trueDetectValue::td_ge10 + 1;
const trueDetectValue td_gateHigh = td_ge1; // Gate high when >= 1V (else low)
const trueDetectValue td_triggerHigh = td_ge1; // Input-trigger High when >= 1V
const trueDetectValue td_triggerLow = td_ge_1; // Input-trigger Low when < 0.1V
const float trueDetectValues[]{ 0.00001f, 0.1f, 0.2f, 0.5f, 1.f, 1.5f, 2.f, 2.5f, 3.f, 4.f, 5.f, 8.f, 10.f };

inline std::string getTrueDetectVoltName(trueDetectValue voltIdx) {
	switch (voltIdx) {
	case td_gt0: return ">0V";
	case td_ge_1: return ">=0.1V";
	case td_ge_2: return ">=0.2V";
	case td_ge_5: return ">=0.5V";
	case td_ge1_5: return ">=1.5V";
	case td_ge2_5: return ">=2.5V";
	default:
		return ">=" + std::to_string((int)trueDetectValues[voltIdx]) + "V";

	}
}

inline std::string getFalseDetectVoltName(trueDetectValue voltIdx) {
	switch (voltIdx) {
	case td_gt0: return "<=0V";
	case td_ge_1: return "<0.1V";
	case td_ge_2: return "<0.2V";
	case td_ge_5: return "<0.5V";
	case td_ge1_5: return "<1.5V";
	case td_ge2_5: return "<2.5V";
	default:
		return "<" + std::to_string((int)trueDetectValues[voltIdx]) + "V";

	}
}

inline std::string getTrueDetectVoltShortName(trueDetectValue voltIdx) {
	switch (voltIdx) {
	case td_gt0: return ">0";
	case td_ge_1: return ">=0.1";
	case td_ge_2: return ">=0.2";
	case td_ge_5: return ">=0.5";
	case td_ge1_5: return ">=1.5";
	case td_ge2_5: return ">=2.5";
	default:
		return ">=" + std::to_string((int)trueDetectValues[voltIdx]);
	}
}

inline std::string getFalseDetectVoltShortName(trueDetectValue voltIdx) {
	switch (voltIdx) {
	case td_gt0: return "<=0";
	case td_ge_1: return "<0.1";
	case td_ge_2: return "<0.2";
	case td_ge_5: return "<0.5";
	case td_ge1_5: return "<1.5";
	case td_ge2_5: return "<2.5";
	default:
		return "<" + std::to_string((int)trueDetectValues[voltIdx]);
	}
}

inline std::vector<std::string> getTrueDetectVoltNames() {
	std::vector<std::string> names;
	for (int i = 0; i < trueDetectValueCount; i++)
		names.push_back(getTrueDetectVoltName((trueDetectValue)i));

	return names;
}

inline std::vector<std::string> getFalseDetectVoltNames() {
	std::vector<std::string> names;
	for (int i = 0; i < trueDetectValueCount; i++)
		names.push_back(getFalseDetectVoltName((trueDetectValue)i));

	return names;
}

inline std::vector<std::string> getTrueDetectVoltShortNames() {
	std::vector<std::string> names;
	for (int i = 0; i < trueDetectValueCount; i++)
		names.push_back(getTrueDetectVoltName((trueDetectValue)i));

	return names;
}

inline std::vector<std::string> getFalseDetectVoltShortNames() {
	std::vector<std::string> names;
	for (int i = 0; i < trueDetectValueCount; i++)
		names.push_back(getFalseDetectVoltName((trueDetectValue)i));

	return names;
}


//-----------------------------------------------------------------------------
// Default Volt-ranges (from/to volt-ranges - e.g. used for clipping-range)
//-----------------------------------------------------------------------------
// mp="minus/plus" (e.g. "-/+5"), zt="zero to" (e.g. "0-5")
enum voltRange {
	vr_off,
	vr_mp50, vr_mp20, vr_mp12, vr_mp10, vr_mp5, vr_mp2_5, vr_mp2, vr_mp1, vr_mp_0_5,
	vr_zt50, vr_zt20, vr_zt12, vr_zt10, vr_zt5, vr_zt2_5, vr_zt2, vr_zt1
};
const int voltRangeCount = voltRange::vr_zt1 + 1;
const voltRange vr_Bipolar = vr_mp5;  // -5V to +5V
const voltRange vr_Unipolar = vr_zt10; // 0V to +10V
// vr_off: span less than2^24 (16777216), min/max ±8388608 — largest range with exact integer rep in float
const float voltRangeMin[]{
	-8000000.f,
	-50.f, -20.f, -12.f, -10.f, -5.f, -2.5f, -2.f, -1.f, -0.5f,
	0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f
};
const float voltRangeMax[]{
	8000000.f,
	50.f, 20.f, 12.f, 10.f, 5.f, 2.5f, 2.f, 1.f, 0.5f,
	50.f, 20.f, 12.f, 10.f, 5.f, 2.5f, 2.f, 1.f
};
const float voltRangeSpan[]{
	16000000.f,
	100.f, 40.f, 24.f, 20.f, 10.f, 5.f, 4.f, 2.f, 1.f,
	50.f, 20.f, 12.f, 10.f, 5.f, 2.5f, 2.f, 1.f
};
// How much to scale/offset a normalized normalized value (0-1), to fit/fill the range
const float voltRangeNormScale[]{
	1.f,
	100.f, 40.f, 24.f, 20.f, 10.f, 5.f, 4.f, 2.f, 1.f,
	50.f, 20.f, 12.f, 10.f, 5.f, 2.5f, 2.f, 1.f
};
const float voltRangeNormOffset[]{
	0.f,
	-50.f, -20.f, -12.f, -10.f, -5.f, -2.5f, -2.f, -1.f, -0.5f,
	0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f
};

// Brightness for clipping-range light (indices match voltRange enum)
const float clipRangeGreenBrightness[]{
	// vr_off
	0.f,
	// bipolar ranges
	1.f,   // vr_mp50: green 100%
	0.5f,  // vr_mp20: green 50%
	0.5f,  // vr_mp12: yellow 50% (with red)
	1.f,   // vr_mp10: yellow 100% (with red)
	0.f,   // vr_mp5:  red dominant
	0.f,   // vr_mp2_5
	0.f,   // vr_mp2
	0.f,   // vr_mp1
	0.f,   // vr_mp_0_5: red only
	// unipolar ranges
	1.f,   // vr_zt50: green 100%
	0.5f,  // vr_zt20: green 50%
	0.5f,  // vr_zt12: yellow 50% (with red)
	1.f,   // vr_zt10: yellow 100% (with red)
	0.f,   // vr_zt5:  red dominant
	0.f,   // vr_zt2_5
	0.f,   // vr_zt2
	0.f    // vr_zt1: red only
};

const float clipRangeRedBrightness[]{
	// vr_off
	0.f,
	// bipolar ranges
	0.f,   // vr_mp50
	0.f,   // vr_mp20
	0.5f,  // vr_mp12: yellow 50%
	1.f,   // vr_mp10: yellow 100%
	0.5f,  // vr_mp5:  about 50% red
	0.6f,  // vr_mp2_5
	0.7f,  // vr_mp2
	0.8f,  // vr_mp1
	1.f,   // vr_mp_0_5: red 100%
	// unipolar ranges
	0.f,   // vr_zt50
	0.f,   // vr_zt20
	0.5f,  // vr_zt12: yellow 50%
	1.f,   // vr_zt10: yellow 100%
	0.5f,  // vr_zt5:  about 50% red
	0.7f,  // vr_zt2_5
	0.8f,  // vr_zt2
	1.f    // vr_zt1: red 100%
};

inline std::string getVoltRangeName(voltRange rangeIdx) {
	if (rangeIdx == vr_mp5)
		return "-5V to 5V (bipolar)";
	if (rangeIdx == vr_zt10)
		return "0V to 10V (unipolar)";
	if (rangeIdx == vr_mp_0_5)
		return "-0.5V to 0.5V";
	if (rangeIdx == vr_mp2_5)
		return "-2.5V to 2.5V";
	if (rangeIdx == vr_zt2_5)
		return "0V to 2.5V";
	return (rangeIdx == vr_off)
		? "Off"
		: std::to_string((int)voltRangeMin[rangeIdx]) + "V to " +
		std::to_string((int)voltRangeMax[rangeIdx]) + "V";
}

inline std::string getClipRangeLightName(voltRange rangeIdx) {
	return std::string("Clipping range: ") + getVoltRangeName(rangeIdx);
}

inline std::string getVoltRangeShortName(voltRange rangeIdx) {
	if (rangeIdx == vr_mp_0_5)
		return "-+0.5";
    if (rangeIdx == vr_mp2_5)
		return "-+2.5";
	if (rangeIdx == vr_zt2_5)
		return "0-2.5";
	return (rangeIdx == vr_off)
		? "Off"
		: std::string((voltRangeMin[rangeIdx] < 0.f)
			? "-+" + std::to_string((int)voltRangeMax[rangeIdx])
			: "0-" + std::to_string((int)voltRangeMax[rangeIdx]));
}

/// @brief Get a list of voltRange-names (e.g. "-5V to 5V") for a popup menu
/// @param inclOff if true, include "Off" in the list
/// @return List of voltRange-names for a popup menu
inline std::vector<std::string> getVoltRangesNames(bool inclOff) {
	std::vector<std::string> names;
	for (int i = inclOff ? 0 : 1; i < voltRangeCount; i++)
		names.push_back(getVoltRangeName((voltRange)i));

	return names;
}

/// @brief Get a list of short voltRange-names (e.g. "-+5") for a popup menu
/// @param inclOff if true, include "Off" in the list
/// @return List of short voltRange-names for a popup menu
inline std::vector<std::string> getVoltRangesShortNames(bool inclOff) {
	std::vector<std::string> names;
	for (int i = inclOff ? 0 : 1; i < voltRangeCount; i++)
		names.push_back(getVoltRangeShortName((voltRange)i));

	return names;
}

/// @brief Clip a value to a given voltRange (specified by index)
/// @param value Value to clip
/// @param rangeIdx voltRange index
/// @return Clipped value
inline float clipToVoltRange(float value, voltRange rangeIdx) {
	return clamp(value, voltRangeMin[rangeIdx], voltRangeMax[rangeIdx]);
}

/// @brief Convert a value from one voltRange to another
/// @param value Value to convert (scale from fromRange to toRange)
/// @param fromRange voltRange (index) to convert from (e.g. 0 to 10V)
/// @param toRange voltRange (index) to convert to (e.g. -5V to 5V)
/// @return Scale value (from fromRange to toRange)
inline float convertVoltRange(float value, voltRange fromRange, voltRange toRange) {
	return (fromRange == vr_off || toRange == vr_off)
		? value
		: (value - voltRangeMin[fromRange]) / voltRangeSpan[fromRange] *
		voltRangeSpan[toRange] + voltRangeMin[toRange];
}


//-----------------------------------------------------------------------------
// Default Volt-inversion-ranges (from/to volt-ranges used for inversion)
//-----------------------------------------------------------------------------
// mp="minus/plus" (invert bipolar), zt="zero to" (various unipolar)
enum voltInvRange { vir_mp, vir_zt12, vir_zt10, vir_zt5, vir_zt4, vir_zt3, vir_zt2, vir_zt1 };
const int voltInvRangeCount = voltInvRange::vir_zt1 + 1;
const float voltInvRangeMax[]{ 1000.f, 12.f, 10.f, 5.f, 4.f, 3.f, 2.f, 1.f };

inline std::string getVoltInvRangeName(voltInvRange rangeIdx) {
	return (rangeIdx == vir_mp)
		? "-/+ (bipolar)"
		: "0V to " + std::to_string((int)voltInvRangeMax[rangeIdx]) + "V";
}

inline std::string getVoltInvRangeShortName(voltInvRange rangeIdx) {
	return (rangeIdx == vir_mp)
		? "-+"
		: "0-" + std::to_string((int)voltInvRangeMax[rangeIdx]);
}

/// @brief Get a list of voltRange-names (e.g. "-5V to 5V") for a popup menu
/// @return List of voltRange-names for a popup menu
inline std::vector<std::string> getVoltInvRangesNames() {
	std::vector<std::string> names;
	for (int i = 0; i < voltInvRangeCount; i++)
		names.push_back(getVoltInvRangeName((voltInvRange)i));

	return names;
}

/// @brief Get a list of short voltRange-names (e.g. "-+5") for a popup menu
/// @return List of short voltRange-names for a popup menu
inline std::vector<std::string> getVoltInvRangesShortNames() {
	std::vector<std::string> names;
	for (int i = 0; i < voltRangeCount; i++)
		names.push_back(getVoltInvRangeShortName((voltInvRange)i));

	return names;
}

/// @brief Invert a value to a given voltInvRange (specified by index). 
/// @param value Value to invert
/// @param rangeIdx voltInvRange index
/// @return Inverted value
inline float invertToVoltInvRange(float value, voltInvRange rangeIdx) {
	return (rangeIdx == vir_mp)
		? value * -1.f
		: voltInvRangeMax[rangeIdx] - value;
}


//-----------------------------------------------------------------------------
// Polophony-mode: monophonic(1), polyphonic(2-16), auto (auto-detect)
//-----------------------------------------------------------------------------
enum polyphonyMode {
	mono_1, poly_2, poly_3, poly_4, poly_5, poly_6, poly_7, poly_8,
	poly_9, poly_10, poly_11, poly_12, poly_13, poly_14, poly_15, poly_16,
	poly_auto
};
const int polyphonyModeCount = polyphonyMode::poly_auto + 1;
const int polyphonyModeChannels[]{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 1 };

inline std::string getPolyphonyModeName(polyphonyMode modeIdx) {
	return (modeIdx == poly_auto)
		? "Auto"
		: (modeIdx == mono_1)
			? "1 (monophonic)"
			: std::to_string((int)polyphonyModeChannels[modeIdx]) + " channels";
}

inline std::string getPolyphonyModeShortName(polyphonyMode modeIdx) {
	return (modeIdx == poly_auto)
		? "Auto"
		: std::to_string((int)polyphonyModeChannels[modeIdx]);
}

/// @brief Get a list of polyphonyMode-names (e.g. "1 (monophonic)") for a popup menu
/// @param inclAuto if true, include "Auto" in the list (as last item)
/// @return List of polyphonyMode-names for a popup menu
inline std::vector<std::string> getPolyphonyModeNames(bool inclAuto = false) {
	std::vector<std::string> names;
	int countInUse = inclAuto ? polyphonyModeCount : polyphonyModeCount - 1;
	for (int i = 0; i < countInUse; i++)
		names.push_back(getPolyphonyModeName((polyphonyMode)i));

	return names;
}

/// @brief Get a list of short polyphonyMode-names (e.g. "1") for a popup menu
/// @param inclAuto if true, include "Auto" in the list (as last item)
/// @return List of short polyphonyMode-names for a popup menu
inline std::vector<std::string> getPolyphonyModeShortNames(bool inclAuto = false) {
	std::vector<std::string> names;
	int countInUse = inclAuto ? polyphonyModeCount : polyphonyModeCount - 1;
	for (int i = 0; i < countInUse; i++)
		names.push_back(getPolyphonyModeShortName((polyphonyMode)i));

	return names;
}


//-----------------------------------------------------------------------------
// Process-quality (determines cycles between each process)
//-----------------------------------------------------------------------------
enum processQuality { pq_audioRate, pq_highRate, pq_balancedRate, pq_lowRate, pq_veryLowRate };
const int processQualityCount = processQuality::pq_veryLowRate + 1;
const uint32_t processQualityPatterns[]{ 0x00, 0x03, 0x0f, 0x3f, 0xff };
const float processQualityCycles[]{ 1.f, 4.f, 16.f, 64.f, 256.f };
const float processQualityGreenBrightness[]{ 0.f, 1.0f, 1.f, 0.5f, 0.f };
const float processQualityRedBrightness[]  { 0.f, 0.0f, 1.f, 1.0f, 1.f };
const std::string processQualityRateNames[5] = { "PQ: Audio (every cycle)",  "PQ: High (4th cycle)", 
	"PQ: Balanced (16th cycle)", "PQ: Low (64th cycle)", "PQ: Very low (256th cycle)" };
const std::string processQualityNames[5] = { "Audio (every cycle)",  "High (4th cycle)", 
	"Balanced (16th cycle)", "Low (64th cycle)", "Very low (256th cycle)" };
const std::string processQualityShortNames[5] = { "Audio",  "High", "Balanced", "Low", "Very low" };

inline std::vector<std::string> getProcessQualityNames() { //TODO: Remove as processQualityNames should be able to be used directly
	std::vector<std::string> names;
	names.push_back(processQualityNames[pq_audioRate]);
	names.push_back(processQualityNames[pq_highRate]);
	names.push_back(processQualityNames[pq_balancedRate]);
	names.push_back(processQualityNames[pq_lowRate]);
	names.push_back(processQualityNames[pq_veryLowRate]);
	return names;
}

inline std::vector<std::string> getProcessQualityShortNames() {
	std::vector<std::string> names;
	names.push_back("Audio");
	names.push_back("High");
	names.push_back("Ballanced");
	names.push_back("Low");
	names.push_back("Very low");
	return names;
}

/// @brief Will estimate the process-quality from the frequency,
/// where aim is to have at least 2048 process-steps per full waveform
/// (it will switch to audio-rate, when frequency is approx 
/// 23.43Hz @ 48kHz / 21.53Hz @ 44.1kHz).
/// @param sampleRate Current sample-rate of engine (check if > 1)
/// @param frequency e.g. the frequency of an LFO
/// @param Estimated process-quality to use (based on LFO -frequency)
inline processQuality getEstimatedLfoProcessQuality(float sampleRate, float frequency) {
	const float minProcStepPerWf = 2048.f; // We aim at having min 2048 cycles per waveform
	sampleRate = sampleRate > 0.f ? sampleRate : 44100.f;

	// Approx 23.43Hz @ 48kHz, or 21.53Hz @ 44.1kHz
	if (frequency >= sampleRate / minProcStepPerWf)
		return pq_audioRate;

	// Approx 5.85Hz @ 48kHz, or 5.38Hz @ 44.1kHz
	if (frequency >= sampleRate / (minProcStepPerWf * processQualityCycles[pq_highRate]))
		return pq_highRate;

	// Approx 1.46Hz @ 48kHz, or 1.34Hz @ 44.1kHz
	if (frequency >= sampleRate / (minProcStepPerWf * processQualityCycles[pq_balancedRate]))
		return pq_balancedRate;

	// Approx 0.366Hz @ 48kHz, or 0.336Hz @ 44.1kHz
	if (frequency >= sampleRate / (minProcStepPerWf * processQualityCycles[pq_lowRate]))
		return pq_lowRate;

	// Approx 0.091Hz @ 48kHz, or 0.084Hz @ 44.1kHz
	return pq_veryLowRate;
}


//-----------------------------------------------------------------------------
// Quantize related functions
//-----------------------------------------------------------------------------
enum quantizeMode {
	qm_off, qm_note, qm_note2nd, qm_note3rd, qm_note4th, qm_note6th,
	qm_volt1, qm_volt2, qm_volt2_5, qm_volt3_33, qm_volt5
};
const int quantizeModeCount = quantizeMode::qm_volt5 + 1;
const float quantizeModeDiv[] = { 1000.f, 1.f / 12.f, 1.f / 6.f, 1.f / 4.f, 1.f / 3.f, 1.f / 2.f, 1.f, 2.f, 2.5f, 10.f / 3.f, 5.f };
const float quantizeModeHalfDiv[] = { 1000.f, 1.f / 24.f, 1.f / 12.f, 1.f / 8.f, 1.f / 6.f, 1.f / 4.f, 0.5f, 1.f, 1.25f, 10.f / 6.f, 2.5f };

inline std::string quantizeModeToName(quantizeMode mode) {
	switch (mode) {
	case qm_note: return "Nearest note (1/12V)";
	case qm_note2nd: return "Nearest 2 notes (1/6V)";
	case qm_note3rd: return "Nearest 3 notes (1/4V)";
	case qm_note4th: return "Nearest 4 notes (1/3V)";
	case qm_note6th: return "Nearest 6 notes (1/2V)";
	case qm_volt1: return "Nearest 1V (octave)";
	case qm_volt2: return "Nearest 2V";
	case qm_volt2_5: return "Nearest 2.5V";
	case qm_volt3_33: return "Nearest 3.33V";
	case qm_volt5: return "Nearest 5V";
	default: return "Off";
	}
}

inline std::string quantizeModeToShortName(quantizeMode mode) {
	switch (mode) {
	case qm_note: return "note";
	case qm_note2nd: return "2 notes";
	case qm_note3rd: return "3 notes";
	case qm_note4th: return "4 notes";
	case qm_note6th: return "6 notes";
	case qm_volt1: return "1V";
	case qm_volt2: return "2V";
	case qm_volt2_5: return "2.5V";
	case qm_volt3_33: return "3.33V";
	case qm_volt5: return "5V";
	default: return "Off";
	}
}

/// @brief Get a list of quantize-names (e.g. "Nearest 2 notes (1/6V)") for a popup menu
/// @return List of quantize-names for a popup menu
inline std::vector<std::string> getquantizeModeNames() {
	std::vector<std::string> names;
	for (int i = 0; i < quantizeModeCount; i++)
		names.push_back(quantizeModeToName((quantizeMode)i));

	return names;
}

/// @brief Get a list of short quantize-names (e.g. "-+5") for a popup menu
/// @return List of short quantize-names for a popup menu
inline std::vector<std::string> getquantizeModeShortNames() {
	std::vector<std::string> names;
	for (int i = 0; i < quantizeModeCount; i++)
		names.push_back(quantizeModeToShortName((quantizeMode)i));

	return names;
}

/// @brief Quantize the input value to the nearest selected mode.
/// @param value to quantize.
/// @param quantize-mode (off, note, node2nd, node3rd ... octave).
/// @return Qantized value.
inline float quantizeToMode(float value, quantizeMode qMode) {
	if (qMode == qm_off)
		return value;

	// Special case when near a whole number
	float wholeValue = std::roundf(value);
	float absWholeDiff = std::abs(value - wholeValue);
	if (absWholeDiff < quantizeModeHalfDiv[qMode]) 
			return wholeValue;

	// Generic quantization
	float sign = value >= 0 ? 1.f : -1.f;
	float absValue = std::abs(value);
	float floorNote = std::floorf(absValue / quantizeModeDiv[qMode]) * quantizeModeDiv[qMode];
	float diff = absValue - floorNote;
	return (diff < quantizeModeHalfDiv[qMode])
		? floorNote * sign
		: (floorNote + quantizeModeDiv[qMode]) * sign;
}


//-----------------------------------------------------------------------------
// infNoiseOutTrigger
//-----------------------------------------------------------------------------
/// @brief Ensures that a trigger cannot fire before the previous trigger 
/// have finished, or was reset (by default the trigger have 1 ms ON-stage 
/// and 1 ms OFF-stage).
struct infNoiseOutTrigger {
	dsp::PulseGenerator pulse;  // Both ON- and OFF-part of the trigger
	float onDurationSec = 1e-3f;
	float offDurationSec = 1e-3f;

	/// @brief Sets the duration of the On-pulse and Off-pulse.
	/// @param onDurationSec duration in seconds of On-pulse (defaults to 1 ms).
	/// @param offDurationSec duration in seconds of Off-pulse (defaults to 1 ms).
	infNoiseOutTrigger(float onDurationSec = 1e-3f, float offDurationSec = 1e-3f) {
		this->onDurationSec = onDurationSec;
		this->offDurationSec = offDurationSec;
	}

	/// @brief Should be called AFTER process is called.
	/// @return true if able to (re)fire the trigger, otherwise false.
	bool trigger() {
		if (pulse.remaining > 0.f)
			return false;
		
		pulse.trigger(onDurationSec + offDurationSec);
		return true;
	}

	/// @brief Returns true if the trigger is currently active (ON/OFF-stage active).
	/// @return true if the trigger is currently active (ON/OFF-stage active).
	inline bool running() {
		return pulse.remaining > 0.f;
	}

	/// @brief Resets the remaining-time of the trigger 
	/// (resets internal pulse used to track time).
	inline void reset() {
		pulse.reset();
	}

	/// @brief Should be called in every module-process loop to 
	/// update remaining time of the trigger.
	/// @param sampleTime Delta-time since lass process call (typically args.SampleTime).
	/// @return true if trigger is currently active (ON/OFF-stage active).
	inline bool process(float sampleTime) {
		return pulse.process(sampleTime);
	}

	/// @brief Returns true if the trigger is currently "high" (ON-stage active).
	/// @return true if the trigger is currently "high" (ON-stage active).
	inline bool isHigh() {
		return pulse.remaining > offDurationSec;
	}

	/// @brief Returns 10V when the trigger is "high" (ON-stage active), 0V otherwise.
	/// @return 10V when the trigger is "high" (ON-stage active), 0V otherwise.
	inline float getVoltage() {
		return (pulse.remaining > offDurationSec) ? 10.f : 0.f;
	}

	/// @brief Returns 1 when the trigger is "high" (ON-stage active), 0 otherwise.
	/// @return 1 when the trigger is "high" (ON-stage active), 0 otherwise.
	inline float getLight() {
		return (pulse.remaining > offDurationSec) ? 1.f : 0.f;
	}
};


//-----------------------------------------------------------------------------
// infNoiseInEdgeDetector
//-----------------------------------------------------------------------------
/// @brief Edge detector both detects when gate goes high-to-low and low-to-high.
/// Calling process() reports on both edges (low-to-high and high-to-low). So if 
/// process() returns true, the edge of the gate can be determined by isHigh() or isLow().
/// If not specified, the threshold defaults to 1.0. 
struct infNoiseInEdgeDetector {
	bool high = false;
	float highThreshold = 1.0f; // At/above detector switch to high.
	float lowThreshold = 1.0f; // Below detector switch to low.

	/// @brief Sets threshold (defaults to 1.0 if not specified). The trigger 
	/// defaults to being low, when created.
	/// @param highThreshold at/above detector switch to high (default to 1.0). 
	/// @param lowThreshold below detector switch to low (default to 1.0). 
	infNoiseInEdgeDetector(float highThreshold = 1.0f, float lowThreshold = 1.0) {
		high = false;
		this->highThreshold = highThreshold;
		this->lowThreshold = lowThreshold;
	}

	/// @brief Returns true if the trigger is currently high
	/// @return true if the trigger is currently high.
	inline bool isHigh() {
		return high;
	}

	/// @brief Returns true if the trigger is currently low.
	/// @return true if the trigger is currently low.
	inline bool isLow() {
		return !high;
	}

	/// @brief Returns true if edge was detected (going low-to-high or 
	/// high-to-low). If edge was detected new state can be determined by 
	/// isHigh() or isLow().
	/// @param lowThreshold below lowThreshold detector switch to low.
	/// @param highThreshold at/above highThreshold detector switch to high. 
	/// @return true if edge was detected (going low-to-high or high-to-low).
	bool process(float input, float lowThreshold, float highThreshold) {
		if (!high && input >= highThreshold) {
			high = true;
			return true;
		}
		else if (high && input < lowThreshold) {
			high = false;
			return true;
		}
		
		return false;
	}

	/// @brief Returns true if edge was detected (going low-to-high or 
	/// high-to-low). If edge was detected new state can be determined by 
	/// isHigh() or isLow().
	/// @param lowThreshold below lowThreshold detector switch to low.
	/// @param highThreshold at/above highThreshold detector switch to high. 
	/// @return true if edge was detected (going low-to-high or high-to-low).
	inline bool process(float input, float threshold) {
		return process(input, threshold, threshold);
	}

	/// @brief Returns true if edge was detected (going low-to-high or 
	/// high-to-low). If edge was detected new state can be determined by 
	/// isHigh() or isLow(). Uses internal lowThreshold and highThreshold.
	/// @return true if edge was detected (going low-to-high or high-to-low).
	inline bool process(float input) {
		return process(input, lowThreshold, highThreshold);
	}

	/// @brief Returns 10V when the trigger is "high" (ON-stage active), 0V otherwise.
	/// @return 10V when the trigger is "high", 0V otherwise.
	inline float getVoltage() {
		return (high) ? 10.f : 0.f;
	}

	/// @brief Sets the threshold to the same value for both high and low.
	/// @param threshold value to set both high and low threshold to.
	inline void setThreshold(float threshold) {
		highThreshold = threshold;
		lowThreshold = threshold;
	}

	/// @brief Sets the threshold to the specified low and high values.
	/// @param lowThreshold below lowThreshold detector switch to low.
	/// @param highThreshold at/above highThreshold detector switch to high.
	inline void setThreshold(float lowThreshold, float highThreshold) {
		this->lowThreshold = lowThreshold;
		this->highThreshold = highThreshold;
	}

	/// @brief Resets the trigger to low.
	inline void reset() {
		high = false;
	}

	/// @brief Returns 1 when the trigger is "high" (ON-stage active), 0 otherwise.
	/// @return 1 when the trigger is "high", 0 otherwise.
	inline float getLight() {
		return (high) ? 1.f : 0.f;
	}
};


//-----------------------------------------------------------------------------
// infNoiseButtonTrigger
//-----------------------------------------------------------------------------
/// @brief Used to track changes in state of a button (or similar). 
/// E.g. changing states between pressed and released.
struct infNoiseButtonTrigger {
	enum buttonState {
		bt_undefined = -1,
		bt_released = 0,
		bt_pressed = 1
	};	
	buttonState state = bt_undefined;
	buttonState lastState = bt_undefined;
	bool changed = false;

	/// @brief Initializes the button state (defaults to bt_undefined).
	infNoiseButtonTrigger(buttonState initState = bt_undefined) {
		state = initState;
		lastState = initState;
		changed = false;
	}

	/// @brief Returns true if the state changed in last process call.
	bool wasChanged() {
		return changed;
	}

	/// @brief Returns true if the button is pressed.
	bool isPressed() {
		return state == bt_pressed;
	}

	/// @brief Returns true if the button is released (not pressed).
	bool isReleased() {
		return state == bt_released;
	}

	/// @brief Resets the button state to a new state
	/// (defaults to bt_undefined).
	void reset(buttonState newState = bt_undefined) {
		state = newState;
		lastState = newState;
		changed = false;
	}

	/// @brief Returns true if the button state changed since last call.
	/// @param pressed True if the button is currently pressed.
	bool process(bool pressed) {
		state = (pressed) ? bt_pressed : bt_released;
		changed = (state != lastState);
	 	lastState = state;

		return changed;
	}
};


//-----------------------------------------------------------------------------
// infNoiseDecayValue
//-----------------------------------------------------------------------------
/// @brief Used to track a value which decays over time (e.g. light-brightness).
/// Internally it both trakcs the value that decay over time (according to 
/// decayRate) and when external objects (e.g. lights) should be updated 
/// (according to updateRate).
struct infNoiseDecayValue {
	float decayValue = 0.f;  // Current value (decays over time).
	float decayRate = 0.f;  // Decay rate (per second).
	float updateValue = 0.f;  // When it reaches 0, external objects should be updated.
	float updateRate = 0.f;  // Update rate (per second).

	/// @brief Constructs a new infNoiseDecay with the specified or default values.
	/// @param decayRate Decay-rate specifies how many times per second a value of 1
	/// fully decays to 0 (defaults to 10).
	/// @param updateRate Update-rate specifies how many times per second the update
	/// value decays to 0, and this method returns true (defaults to 30).
	infNoiseDecayValue(float decayRate = 10.f, float updateRate = 30.f) {
		this->decayRate = decayRate;
		this->updateRate = updateRate;
	}

	/// @brief Should be called in every module-process loop to decay the 
	/// decayValue and update the updateValue. Returns true if external
	/// objects (e.g. light) should be updated according to updateRate.
	/// @param sampleTime Delta-time since lass process call (typically args.SampleTime).
	/// @return true external objects (e.g. light) should be updated.
	inline bool process(float sampleTime) {
		decayValue -= decayRate * sampleTime;
		if (decayValue < 0.f) {
			decayValue = 0.f;
		}

		updateValue -= updateRate * sampleTime;
		bool doUpdate = false;
		while (updateValue < 0.f) {
			updateValue += 1.f;
			doUpdate = true;
		}
		return doUpdate;  // True if external object (e.g. light) should be updated.
	}

	/// @brief Sets the decay value as specified (defaults to 1).
	/// @param decayValue Decay value to set (typical 0 to 1).
	/// @param forceUpdate If true, updateValue is set to 0 (force next call to
	/// process to return true).
	inline void setDecayValue(float decayValue = 1.f, bool forceUpdate = false) {
		this->decayValue = decayValue;

		if (forceUpdate) {
			updateValue = 0.f;
		}
	}

	/// @brief Returns the current decay value.
	/// @return Current decay value (typical 0 to 1).
	inline float getDecayValue() {
		return decayValue;
	}

	/// @brief Resets both decayValue and updateValue to 0.
	inline void reset() {
		decayValue = 0.f;
		updateValue = 0.f;
	}
};


//-----------------------------------------------------------------------------
// infNoiseAttRngQnt
//-----------------------------------------------------------------------------
/// @brief Param-quantity for attenuation ranges (1x, 2x, 5x and 10x).
/// Internally the value is stored as a float between -1 and 1, 
/// but scaled accordning to the active range when set/displayed.
struct infNoiseAttRngQnt : ParamQuantity {

	enum attRange { ar_1x, ar_2x, ar_5x, ar_10x };
	const float attRangeFactors[4] = { 1.f, 2.f, 5.f, 10.f };
	attRange range = ar_1x;  // Active range (defaults to 1x).

	/// @brief Constructs a new infNoiseAttRngQnt with the default values
	/// (both physical- and logical range -1 to +1).
	infNoiseAttRngQnt() {
		unit = " x";
		displayMultiplier = 1.f;
		minValue = -1.f;
		maxValue = 1.f;
		defaultValue = 1.f;

		name = getRangeName();
	}

	/// @brief Returns the range factored for the specified range
	/// (e.g. between -2 and +2 for 2x).
	/// @return Logical value (e.g. 2) from physical (e.g. 1).
	float getDisplayValue() override {
		return getValue() * attRangeFactors[(int)range];
	}

	/// @brief Sets physical value (e.g. to 1) when setting a logical
	/// value (e.g. 2).
	/// @param displayValue logical value to set (will set physical -1/+1).	
	void setDisplayValue(float displayValue) override {
		setValue(displayValue / attRangeFactors[(int)range]);
	}

	/// @brief Sets the range to the specified range, and updates the name.
	/// @param range attRange to set.
	/// @param newName New name (defaults to "Scale").
	/// @param appendScaleRange If true, scale added after new name (defaults to true).
	void setRange(attRange range, std::string newName = "Scale", bool appendScaleRange = true) {
		this->range = range;
		name = getRangeName(newName, appendScaleRange);

		// Default to 1V, no matter range.
		defaultValue = 1.f / attRangeFactors[(int)range];
	}

	/// @brief Returns the name of the range (e.g. "Scale (1x to 1x)").
	/// @param newName New name (defaults to "Scale").
	/// @param appendScaleRange If true, scale added after new name (defaults to true).
	/// @return Name of the range (e.g. "Scale (1x to 1x)").
	std::string getRangeName(std::string newName = "Scale", bool appendScaleRange = true) {
		std::string scaleRangeName = (appendScaleRange)
			? " (" + std::to_string((int)-attRangeFactors[(int)range]) + "x" +
			" to + " + std::to_string((int)attRangeFactors[(int)range]) + "x)"
			: "";
		return newName + scaleRangeName;
	}

	/// @brief Sets the lights based on the range 
	/// (1x=off, 2x=green, 5x=yellow, 10x=red).
	/// @param lgtRed index (green=lgtRed+1, blue=lgtRed+2).
	void setRangeLights(Module* module, int lgtRed) {
		//Set all lights to off (used for 1x)
		module->lights[lgtRed].setBrightness(0.0f);  // Red
		module->lights[lgtRed + 1].setBrightness(0.0f);  // Green
		module->lights[lgtRed + 2].setBrightness(0.0f);  // Blue

		// Set lights based on range
		if (range == ar_2x) { // Pure green
			module->lights[lgtRed + 1].setBrightness(1.0f);
		}
		else if (range == ar_5x) { // Yellow "towards" orange
			module->lights[lgtRed].setBrightness(1.0f);
			module->lights[lgtRed + 1].setBrightness(0.8f);
		}
		else if (range == ar_10x) { // Pure red
			module->lights[lgtRed].setBrightness(1.0f);
		}
	}
};


//-----------------------------------------------------------------------------
// infNoisePwmRngQnt
//-----------------------------------------------------------------------------
/// @brief ParamQuantity for multiple PWM (Pulse Width Modulation) ranges.
/// Internally the value is stored as a float between -1 and 1, but 
/// displayed/set according to the selected range (e.g. 1% to 99%).
struct infNoisePwmRngQnt : ParamQuantity {
	enum pwmRange { pwm_01_99, pwm_05_95, pwm_10_90, pwm_15_85, pwm_20_80, pwm_25_75 };
	const float minRanges[6] = { 0.01f, 0.05f, 0.10f, 0.15f, 0.20f, 0.25f };
	const float maxRanges[6] = { 0.99f, 0.95f, 0.90f, 0.85f, 0.80f, 0.75f };
	const float pwmHalfRanges[6] = { 0.49f, 0.45, 0.40f, 0.35f, 0.30f, 0.25f };
	pwmRange range = pwm_01_99;  // Active range (defaults to 1% to 99%).

	/// @brief Constructs a new infNoisePwmRngQnt with default values
	/// (physical range -1 to +1, but logical range from 1 to 99).
	infNoisePwmRngQnt() {
		unit = " %";
		displayMultiplier = 100.f;
		minValue = -1.f;
		maxValue = 1.f;
		defaultValue = 0.f;

		name = getRangeName();
	}

	/// @brief Returns the range factored for the specified range
	/// (e.g. pysical value 1.0 is returned as 99 for pwm_01_99).
	float getDisplayValue() override {
		return rescale(getValue(), -1.f, +1.f, minRanges[range], maxRanges[range]) * 100.f;
	}

	/// @brief Sets physical value (e.g. to 1) when setting a logical 
	/// value like 99, when using pwm_01_99.
	void setDisplayValue(float displayValue) override {
		setValue(rescale(displayValue / 100.f, minRanges[range], maxRanges[range], -1.f, +1.f));
	}

	/// @brief Sets the range to the specified range, and updates the name.
	/// @param range pwmRange to set.
	/// @param newName New name (defaults to "PWM").
	/// @param appendScaleRange If true, scale added after new name (defaults to true).
	void setRange(pwmRange range, std::string newName = "PWM", 
		bool appendScaleRange = true) {
		this->range = range;
		name = getRangeName(newName, appendScaleRange);
	}

	/// @brief Returns the name of the range (e.g. "PWM (1% - 99%)").
	/// @param newName New name (defaults to "Scale").
	/// @param appendScaleRange If true, scale added after new name (defaults to true).
	/// @return Name of the range (e.g. "Scale (1x to 1x)").
	std::string getRangeName(std::string newName = "PWM", 
		bool appendScaleRange = true) {
		std::string scaleRangeName = (appendScaleRange)
			? " (" + std::to_string((int)(100.f * (0.5f - pwmHalfRanges[(int)range]) + 0.5)) + "\%" +
			" / " + std::to_string((int)(100.f * (0.5f + pwmHalfRanges[(int)range]) + 0.5)) + "\%)"
			: "";
		return newName + scaleRangeName;
	}
};


//-----------------------------------------------------------------------------
// infNoiseLfoFreqQnt
//-----------------------------------------------------------------------------
/// @brief ParamQuantity for LFO frequency.
struct infNoiseLfoFreqQnt : ParamQuantity {
	float getDisplayValue() override {
		unit = " Hz";
		displayMultiplier = 1.f;
		return ParamQuantity::getDisplayValue();
	}
};


//-----------------------------------------------------------------------------
// infNoiseRngMaxQnt
//-----------------------------------------------------------------------------
/// @brief ParamQuantity for a knob which can either be used to change range- 
/// value (0V to 10V) or max-value (-10V to 10V). Internally the value is 0 to 1, 
/// but displayed accordingly to selected mode (range or max).
struct infNoiseRngMaxQnt : ParamQuantity {
	enum rngMaxMode { rmpq_Range, rmpq_Max };
	rngMaxMode mode = rmpq_Range;

	infNoiseRngMaxQnt() {
		unit = " v";
		displayMultiplier = 1.f;
		minValue = 0.f;
		maxValue = 1.f;
		defaultValue = 1.f;

		setMode(rmpq_Range);
	}

	float getDisplayValue() override {
		if (mode == rmpq_Range) {
			return getValue() * 10.f;
		}

		return getValue() * 20.f - 10.f;
	}

	void setDisplayValue(float displayValue) override {
		if (mode == rmpq_Range) {
			setValue(displayValue / 10.f);
		}
		else {
			setValue((displayValue + 10.f) / 20.f);
		}
	}

	void setMode(rngMaxMode mode) {
		this->mode = mode;
		if (mode == rmpq_Range) {
			name = "Range (0V to 10V)";
			defaultValue = 1.f; // 10V
		}
		else {
			name = "Max (-10V to 10V)";
			defaultValue = 0.75f; // 5V
		}
	}
};


//-----------------------------------------------------------------------------
// infNoiseCntrMinQnt
//-----------------------------------------------------------------------------
/// @brief ParamQuantity for a knob which can either be used to change center- 
/// or min-value (both -10V to 10V). Internally the value is 0 to 1, but 
/// displayed accordingly to selected mode (center or min).
struct infNoiseCntrMinQnt : ParamQuantity {
	enum centerMinMode { cmpq_Center, cmpq_Min };
	centerMinMode mode = cmpq_Center;

	infNoiseCntrMinQnt() {
		unit = " v";
		displayMultiplier = 1.f;
		minValue = 0.f;
		maxValue = 1.f;
		defaultValue = 1.f;

		setMode(cmpq_Center);
	}

	float getDisplayValue() override {
		return getValue() * 20.f - 10.f;
	}

	void setDisplayValue(float displayValue) override {
		setValue((displayValue + 10.f) / 20.f);
	}

	void setMode(centerMinMode mode) {
		this->mode = mode;
		if (mode == cmpq_Center) {
			name = "Center (-10V to 10V)";
			defaultValue = 0.5f; // 0V
		}
		else {
			name = "Min (-10V to 10V)";
			defaultValue = 0.25f; // -5V
		}
	}
};


//-----------------------------------------------------------------------------
// autoScaleData
//-----------------------------------------------------------------------------
/// @brief autoScaleData contains data for auto-scaling a signal, based on
/// observed min/max values. The scale and offset is calculated based on
/// the observed min/max values, and can be used to scale the signal.
struct autoScaleData {
	float minSignal = 0.0f; // Lowest signal received
	float maxSignal = 0.0f;  // Highest signal received
	float scale = 0.f;  // Calculated scale
	float offset = 0.f;  // Calculated offset
	bool wasReset = true;  // True after reset (until updateScaleOffset is called)

	/// @brief Get the scaled value of a raw-signal, based on the scale and offset,
	/// as calculated by the updateScaleOffset method.
	/// @param value Raw signal value
	inline float getScaledValue(float value) {
		return value * scale + offset;
	}

	/// @brief Get the raw value of a scaled signal, based on the scale and offset
	/// (basically the inverse of getScaledValue).
	inline float getRawValue(float value) {
		return (scale != 0.f)
			? (value - offset) / scale
			: 0.f;
	}

	/// @brief Update the scale and offset based on a new signal value
	/// and specified min/max (definin output range).
	/// @param value Raw signal value (before scaling)
	/// @param minRange Minimum output range (used for scaling and offset)
	/// @param maxRange Maximum output range (used for scaling and offset)
	void updateScaleOffset(float value, float minRange, float maxRange) {
		if (!wasReset) {
			if (value < minSignal) {
				minSignal = value;
			}
			if (value > maxSignal) {
				maxSignal = value;
			}
		}
		else {
			minSignal = value;
			maxSignal = value;
			wasReset = false;
		}

		scale = (minSignal != maxSignal)
			? fabs(maxRange - minRange) / (maxSignal - minSignal)
			: 1.f;
		offset = minRange - minSignal * scale;
	}

	/// @brief Get the mid-point of the signal between minSignal and maxSignal
	/// (mid = (minl + max) / 2)
	inline float getMidSignal() {
		return (minSignal + maxSignal) / 2.f;
	}

	/// @brief Get the range of the signal between maxSignal and minSignal
	/// (range=(max-min))
	inline float getSignalRange() {
		return fabs(maxSignal - minSignal);
	}

	/// @brief Reset the autoScaleData (e.g. after a new signal is connected)
	void reset() {
		minSignal = 0.0f;
		maxSignal = 0.0f;
		wasReset = true;
		scale = 1.f;
		offset = 0.f;
	}

	/// @brief Load auto-scaling data from a JSON object.
	void Load(json_t* rootJ, const std::string& prefix) {
		minSignal = getJsonFloat(rootJ, (prefix + "_min").c_str(), 0.0f);
		maxSignal = getJsonFloat(rootJ, (prefix + "_max").c_str(), 0.0f);
		wasReset = getJsonBool(rootJ, (prefix + "_rst").c_str(), true);
		scale = getJsonFloat(rootJ, (prefix + "_scl").c_str(), 1.0f);
		offset = getJsonFloat(rootJ, (prefix + "_ofs").c_str(), 0.0f);
	}

	/// @brief Save auto-scaling data to a JSON object.
	void Save(json_t* rootJ, const std::string& prefix) {
		json_object_set_new(rootJ, (prefix + "_min").c_str(), json_real(minSignal));
		json_object_set_new(rootJ, (prefix + "_max").c_str(), json_real(maxSignal));
		json_object_set_new(rootJ, (prefix + "_rst").c_str(), json_boolean(wasReset));
		json_object_set_new(rootJ, (prefix + "_scl").c_str(), json_real(scale));
		json_object_set_new(rootJ, (prefix + "_ofs").c_str(), json_real(offset));
	}
};


//-----------------------------------------------------------------------------
// Scale curve (exp / linear / log) — DSP shaping and GreenRed indicator
//-----------------------------------------------------------------------------
/// Response shape for normalized values (e.g. VCA gain 0–1, Tweak scale −1–1).
/// Integer order: Exp (0) — Linear (1) — Log (2) (power-law spectrum x^p with p>1, p=1, p<1).
enum scaleCurve {
	sc_exp,
	sc_linear,
	sc_log
};
const int scaleCurveCount = scaleCurve::sc_log + 1;

/// @brief Map \p x with exponential (x²), linear, or logarithmic (√x) curve (unipolar). Callers should pass x ≥ 0 (e.g. clamped gain).
inline float applyScaleCurveUnipolar(float x, scaleCurve mode) {
	switch (mode) {
	case sc_exp: return x * x;
	case sc_log: return std::sqrt(x);
	case sc_linear:
	default:
		return x;
	}
}

/// @brief Same curves on |x|, sign preserved (bipolar −1…1).
inline float applyScaleCurveSigned(float x, scaleCurve mode) {
	if (mode == sc_linear)
		return x;
	float sign = (x < 0.f) ? -1.f : 1.f;
	return sign * applyScaleCurveUnipolar(std::fabs(x), mode);
}

/// @brief Drive a GreenRed light pair: green = `lights[lightId]`, red = `lights[lightId + 1]`. Exp = green, Log = red, Linear = both off.
inline void setScaleModeLight(Module* module, int lightId, scaleCurve mode, float brightness = 1.f) {
	module->lights[lightId].setBrightness(0.f);
	module->lights[lightId + 1].setBrightness(0.f);
	switch (mode) {
	case sc_exp:
		module->lights[lightId].setBrightness(brightness);
		break;
	case sc_log:
		module->lights[lightId + 1].setBrightness(brightness);
		break;
	case sc_linear:
	default:
		break;
	}
}

/// Labels for scaleCurve menus: Exp, Linear, Log (matches enum order). Use with Rack’s
/// `createIndexPtrSubmenuItem(menuTitle, getScaleCurveMenuNames(), &myActReqValue.req)` — no extra wrappers required.
inline const std::vector<std::string>& getScaleCurveMenuNames() {
	static const std::vector<std::string> k{ "Exp", "Linear", "Log" };
	return k;
}


//-----------------------------------------------------------------------------
// Misc
//-----------------------------------------------------------------------------
/// @brief Returns the brightness of a light based on frequency, phase
/// and if the light is active (some module can disable the light).
/// @param freq Frequency to base brightness on.
/// @param phase Phase (0-1) to base brightness on.
/// @param lightActive True if the light is active (defaults to true).
inline float getFreqPhaseBrightness(float freq, float phase, bool lightActive = true) {
	if (!lightActive)
		return 0.f;
	
	if (freq >= 60.f)
		return 1.0f;
	
	return phase < 0.75f 
		? 1.f - phase 
		: 0.f;
}

/// @brief It appears on (VCV)Windows that the sample-rate can be 0 on first call.
/// This method defaults to 44100 if sample-rate is 0, to prevent 
/// div-by-zero in modules using sample-rate.
/// @param sampleRate Sample-rate to check.
inline float safeSampleRate(float sampleRate) {
	return (sampleRate > 0.f) ? sampleRate : 44100.f;
}
