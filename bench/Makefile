EXE ?= latrunculi
CXX ?= c++
NNUE ?= OFF
NNUE_BACKEND ?= AUTO
NNUE_NETWORK ?= $(abspath ../networks/nn-82215d0fd0df.nnue)

.PHONY: all clean

all:
	cmake -S .. -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=$(CXX) -DLATRUNCULI_NNUE=$(NNUE) -DLATRUNCULI_NNUE_BACKEND=$(NNUE_BACKEND) -DLATRUNCULI_NNUE_NETWORK="$(NNUE_NETWORK)"
	cmake --build build --target latrunculi --parallel 2
	cmake -E copy build/latrunculi $(EXE)

clean:
	cmake -E rm -rf build $(EXE)
