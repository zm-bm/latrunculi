EXE ?= latrunculi
CXX ?= c++
# OpenBench names executables <engine>-<commit>[-<network>].
TUNING ?= $(if $(filter Latrunculi-Tuning-%,$(notdir $(EXE))),ON,OFF)
BUILD_DIR := ../build/openbench

.PHONY: all clean

all:
	cmake -S .. -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=$(CXX) -DLATRUNCULI_TUNING=$(TUNING)
	cmake --build $(BUILD_DIR) --target latrunculi --parallel 2
	cmake -E copy $(BUILD_DIR)/latrunculi $(EXE)

clean:
	cmake -E rm -rf $(BUILD_DIR) $(EXE)
