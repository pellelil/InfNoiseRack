#Set RACK_DIR if building with Rack
RACK_DIR ?= ../..

#Set RACK_DIR if building with Rack SDK
#RACK_DIR ?= C:/Data/Cpp/Rack-SDK

# Specify extra directories to search for include files.
FLAGS += -I./src/ctrl

# Add .cpp and .c files to the build
# This says "all cpp files are in the src folder. You can add more files
# to that folder and they will get compiled and linked also.
SOURCES += $(wildcard src/*.cpp)

DISTRIBUTABLES += res
# DISTRIBUTABLES += presets
# DISTRIBUTABLES += selections

# Strip UTF-8 BOM from plugin.json if an editor/agent added one (VCV Rack cannot load manifest with BOM).
ifeq ($(OS),Windows_NT)
$(shell powershell -NoProfile -ExecutionPolicy Bypass -File scripts/ensure-plugin-json-no-bom.ps1 >/dev/null 2>&1)
endif

# Include the VCV Rack plugin Makefile framework
include $(RACK_DIR)/plugin.mk

## change c++ version here, after others have been included
#CXXFLAGS := $(filter-out -std=c++11,$(CXXFLAGS))
#CXXFLAGS += -std=c++17