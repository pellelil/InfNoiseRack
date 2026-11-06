#Set RACK_DIR if building with Rack
RACK_DIR ?= ../..

#Set RACK_DIR if building with Rack SDK
#RACK_DIR ?= C:/Data/Cpp/Rack-SDK

# Specify extra directories to search for include files.
FLAGS += -I./src/in
FLAGS += -I./src/ctrl

# Modules live in src/. Shared base (src/in/) is compiled here too.
SOURCES += $(wildcard src/*.cpp)
SOURCES += $(wildcard src/in/*.cpp)

DISTRIBUTABLES += res
# DISTRIBUTABLES += presets
# DISTRIBUTABLES += selections

# Include the VCV Rack plugin Makefile framework
include $(RACK_DIR)/plugin.mk

## change c++ version here, after others have been included
#CXXFLAGS := $(filter-out -std=c++11,$(CXXFLAGS))
#CXXFLAGS += -std=c++17