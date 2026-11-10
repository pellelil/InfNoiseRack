// Infinite-Noise modules (c) 2024-2026 Pelle Liljendal
// Licensed under GNU GPLv3+

#pragma once

#include "plugin.hpp"

// Shared stack buffer for Stack and Stack bank expander.
// Index 0 is the oldest stored item. The Size knob is a window on the newest
// items: `hidden` items at the front stay stored but are not popped, cycled,
// or shown. The window only moves when Size changes, so a Pop does not pull a
// hidden item into view.
struct InfNoiseStackData {
	static const int kSlots = 16;

	struct Frame {
		float v[PORT_MAX_CHANNELS];
		int channels;
		uint32_t id; // 0 = none. Assigned on push and on every slot written by Send.
	};

	Frame frames[kSlots];
	int stored = 0;       // Items in memory, including those hidden by Size
	int hidden = 0;       // frames[0 .. hidden-1] are outside the Size window
	int appliedSize = 16; // Size last applied, so the knob is handled as a delta
	uint32_t nextId = 1;

	int accessible() const {
		return stored - hidden;
	}

	void clear() {
		stored = 0;
		hidden = 0;
		for (int i = 0; i < kSlots; i++) {
			frames[i].channels = 0;
			frames[i].id = 0;
		}
	}

	uint32_t allocId() {
		if (nextId == 0)
			nextId = 1;
		uint32_t id = nextId++;
		if (nextId == 0)
			nextId = 1;
		return id;
	}

	void applySize(int newSize) {
		newSize = clamp(newSize, 1, kSlots);
		if (newSize == appliedSize)
			return;

		int acc = stored - hidden;
		if (newSize < appliedSize) {
			int hide = acc - newSize;
			if (hide > 0)
				hidden += hide;
		}
		else {
			int reveal = newSize - acc;
			reveal = clamp(reveal, 0, hidden);
			hidden -= reveal;
		}
		hidden = clamp(hidden, 0, stored);
		appliedSize = newSize;
	}

	void dropIndex(int idx) {
		if (idx < 0 || idx >= stored)
			return;
		for (int i = idx + 1; i < stored; i++)
			frames[i - 1] = frames[i];
		stored--;
		if (idx < hidden)
			hidden--;
		if (hidden > stored)
			hidden = stored;
	}

	// Full means the accessible window already holds `size` items.
	// Rolling drops the oldest accessible item (hidden items stay). When the
	// window is not full but all 16 slots are used, the oldest hidden item is
	// dropped to make room.
	bool tryPush(int size, bool rolling, Frame in, bool& wasFull) {
		size = clamp(size, 1, kSlots);
		int acc = accessible();
		wasFull = acc >= size;
		if (wasFull) {
			if (!rolling)
				return false;
			dropIndex(hidden);
		}
		else if (stored >= kSlots) {
			dropIndex(0);
		}
		if (stored >= kSlots)
			return false;
		in.id = allocId();
		frames[stored++] = in;
		return true;
	}

	// fifo: oldest accessible. Otherwise newest.
	bool pop(bool fifo, Frame& out) {
		if (accessible() <= 0)
			return false;
		int idx = fifo ? hidden : (stored - 1);
		out = frames[idx];
		dropIndex(idx);
		return true;
	}

	// Accessible index 0 is the oldest item still inside Size.
	int findAccessible(uint32_t id) const {
		if (id == 0)
			return -1;
		int acc = accessible();
		for (int i = 0; i < acc; i++) {
			if (frames[hidden + i].id == id)
				return i;
		}
		return -1;
	}

	const Frame* accessibleAt(int accIndex) const {
		if (accIndex < 0 || accIndex >= accessible())
			return nullptr;
		return &frames[hidden + accIndex];
	}

	void replaceAll(const InfNoiseStackData& src, int size) {
		size = clamp(size, 1, kSlots);
		Frame tmp[kSlots];
		int n = src.stored;
		n = clamp(n, 0, kSlots);
		for (int i = 0; i < n; i++)
			tmp[i] = src.frames[i];
		clear();
		for (int i = 0; i < n; i++) {
			frames[i] = tmp[i];
			frames[i].id = allocId();
		}
		stored = n;
		hidden = 0;
		appliedSize = n > 0 ? n : size;
		applySize(size);
	}

	void copyAccessibleTo(InfNoiseStackData& dst) const {
		Frame tmp[kSlots];
		int acc = clamp(accessible(), 0, kSlots);
		acc = clamp(acc, 0, kSlots);
		for (int i = 0; i < acc; i++)
			tmp[i] = frames[hidden + i];
		dst.clear();
		for (int i = 0; i < acc; i++) {
			dst.frames[i] = tmp[i];
			dst.frames[i].id = dst.allocId();
		}
		dst.stored = acc;
		dst.hidden = 0;
		dst.appliedSize = kSlots;
	}

	void writeJson(json_t* rootJ, const char* countKey, const char* channelsKey, const char* valuesKey) const {
		int chans[kSlots];
		float values[kSlots * PORT_MAX_CHANNELS];
		int valueCount = 0;
		for (int i = 0; i < kSlots; i++) {
			if (i < stored) {
				int ch = clamp(frames[i].channels, 0, PORT_MAX_CHANNELS);
				chans[i] = ch;
				for (int c = 0; c < ch; c++)
					values[valueCount++] = frames[i].v[c];
			}
			else {
				chans[i] = 0;
			}
		}
		json_object_set_new(rootJ, countKey, json_integer(stored));
		setJsonIntArray(rootJ, channelsKey, chans, kSlots);
		setJsonFloatArray(rootJ, valuesKey, values, (size_t)valueCount);
	}

	// Loads oldest-first, assigns fresh ids, then hides whatever does not fit Size.
	void readJson(json_t* rootJ, const char* countKey, const char* channelsKey, const char* valuesKey, int sizeKnob) {
		clear();
		int count = clamp(getJsonInt(rootJ, countKey, 0), 0, kSlots);
		int chans[kSlots];
		getJsonIntArray(rootJ, channelsKey, chans, kSlots, 0);
		float values[kSlots * PORT_MAX_CHANNELS];
		getJsonFloatArray(rootJ, valuesKey, values, kSlots * PORT_MAX_CHANNELS, 0.f);

		int p = 0;
		for (int i = 0; i < count; i++) {
			int ch = chans[i];
			ch = clamp(ch, 0, PORT_MAX_CHANNELS);
			frames[i].channels = ch;
			frames[i].id = allocId();
			for (int c = 0; c < PORT_MAX_CHANNELS; c++)
				frames[i].v[c] = 0.f;
			for (int c = 0; c < ch; c++) {
				if (p < kSlots * PORT_MAX_CHANNELS)
					frames[i].v[c] = values[p++];
			}
		}
		stored = count;
		hidden = 0;
		sizeKnob = clamp(sizeKnob, 1, kSlots);
		appliedSize = stored > 0 ? stored : sizeKnob;
		applySize(sizeKnob);
	}
};

struct InfNoiseStackModule : InfNoiseModule {
	InfNoiseStackData stack;
	int sizeKnob = 16;
	bool pushOverflow = false;  // Set for this sample when Push arrives and the window is full
	bool popUnderflow = false;  // Set for this sample when Pop arrives and nothing is accessible

	void writePort(engine::Output& port, const float* values, int srcChannels, int outChannels) {
		outChannels = clamp(outChannels, 1, PORT_MAX_CHANNELS);
		srcChannels = clamp(srcChannels, 0, PORT_MAX_CHANNELS);
		port.setChannels(outChannels);
		for (int c = 0; c < outChannels; c++) {
			float v = (values && c < srcChannels) ? values[c] : 0.f;
			v = quantizeToMode(v, outQuantize.act);
			v = clipToVoltRange(v, outClipRange.act, outClipMode.act);
			port.setVoltage(v, c);
		}
	}

	void pushToExpanders();
};

void infNoiseStackExpander_exchange(Module* neighbor, InfNoiseStackModule* host);
void infNoiseStackBank_exchange(Module* neighbor, InfNoiseStackModule* host);

// Expander side select, same rules as Envelope Phase Expander.
struct InfNoiseStackLink {
	enum Mode {
		Auto = 0,
		ForcedLeft = 1,
		ForcedRight = 2
	};

	actReqValue<int> mode;
	int connectedSide; // -1 none, 0 left host, 1 right host

	InfNoiseStackLink() : mode(Auto), connectedSide(-1) {}

	static bool isHost(Module* m) {
		return m && m->model == modelStack;
	}

	void update(Module* self) {
		bool left = isHost(self->getLeftExpander().module);
		bool right = isHost(self->getRightExpander().module);
		connectedSide = -1;
		if (mode.act == ForcedLeft) {
			if (left)
				connectedSide = 0;
		}
		else if (mode.act == ForcedRight) {
			if (right)
				connectedSide = 1;
		}
		else if (left && !right) {
			connectedSide = 0;
		}
		else if (right && !left) {
			connectedSide = 1;
		}
	}

	void updateLights(Module* self, int leftLight, int rightLight) {
		bool left = isHost(self->getLeftExpander().module);
		bool right = isHost(self->getRightExpander().module);
		self->lights[leftLight].setBrightness(0.f);
		self->lights[leftLight + 1].setBrightness(0.f);
		self->lights[rightLight].setBrightness(0.f);
		self->lights[rightLight + 1].setBrightness(0.f);

		if (mode.act == Auto) {
			if (left && right) {
				self->lights[leftLight + 1].setBrightness(1.f);
				self->lights[rightLight + 1].setBrightness(1.f);
			}
			else if (left) {
				self->lights[leftLight].setBrightness(1.f);
			}
			else if (right) {
				self->lights[rightLight].setBrightness(1.f);
			}
			else {
				self->lights[leftLight + 1].setBrightness(1.f);
				self->lights[rightLight + 1].setBrightness(1.f);
			}
		}
		else if (mode.act == ForcedLeft) {
			self->lights[leftLight + (left ? 0 : 1)].setBrightness(1.f);
		}
		else {
			self->lights[rightLight + (right ? 0 : 1)].setBrightness(1.f);
		}
	}

	bool accepts(Module* self, Module* from) const {
		if (!from || !self || connectedSide < 0)
			return false;
		if (connectedSide == 0)
			return self->leftExpander.module == from;
		return self->rightExpander.module == from;
	}

	InfNoiseStackModule* host(Module* self) const {
		if (!self || connectedSide < 0)
			return nullptr;
		Module* m = (connectedSide == 0) ? self->leftExpander.module : self->rightExpander.module;
		if (!isHost(m))
			return nullptr;
		return static_cast<InfNoiseStackModule*>(m);
	}
};

inline int infNoisePickIndex(int count) {
	if (count <= 1)
		return 0;
	int pick = (int)(random::uniform() * (float)count);
	if (pick < 0)
		pick = 0;
	if (pick >= count)
		pick = count - 1;
	return pick;
}

// mode: 0 next, 1 prev, 2 ping-pong, 3 random, 4 random different.
// Index 0 is the first item (oldest accessible, or bank 1). Without a current
// item, Next and ping-pong land on 0 and Prev lands on the last.
inline int infNoiseCycleIndex(int current, int count, int mode, int& pingDir, bool haveCurrent) {
	if (count <= 0)
		return -1;
	mode = clamp(mode, 0, 4);
	if (!haveCurrent) {
		if (mode == 1)
			return count - 1;
		if (mode == 3 || mode == 4)
			return infNoisePickIndex(count);
		if (mode == 2)
			pingDir = 1;
		return 0;
	}
	if (count == 1)
		return 0;
	if (mode == 0)
		return (current + 1) % count;
	if (mode == 1)
		return (current - 1 + count) % count;
	if (mode == 2) {
		if (pingDir >= 0 && current == count - 1)
			pingDir = -1;
		else if (pingDir < 0 && current == 0)
			pingDir = 1;
		int next = current + (pingDir >= 0 ? 1 : -1);
		if (next < 0)
			next = 0;
		if (next >= count)
			next = count - 1;
		return next;
	}
	if (mode == 3)
		return infNoisePickIndex(count);
	int next = current;
	for (int i = 0; i < 8 && next == current; i++)
		next = infNoisePickIndex(count);
	if (next == current)
		next = (current + 1) % count;
	return next;
}
