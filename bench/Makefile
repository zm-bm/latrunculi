EXE ?= latrunculi
CXX ?= c++
NNUE_NETWORK := $(abspath ../networks/nn-72ae9ad06b97.nnue)

.PHONY: all clean

all:
	cmake -S .. -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=$(CXX) -DLATRUNCULI_NNUE=ON -DLATRUNCULI_NNUE_NETWORK="$(NNUE_NETWORK)"
	cmake --build build --target latrunculi --parallel 2
	cmake -E copy build/latrunculi $(EXE)

clean:
	cmake -E rm -rf build $(EXE)
