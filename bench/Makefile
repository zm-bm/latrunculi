EXE ?= latrunculi
CXX ?= c++
NNUE_NETWORK := $(abspath ../networks/nn-72ae9ad06b97.nnue)
NNUE_SHA256 := 72ae9ad06b978ee6966334152bb2e3e8daa1e1f9fb2ace31b96a3c208f5633b4

.PHONY: all clean

all:
	cmake -S .. -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=$(CXX) -DLATRUNCULI_NNUE=ON -DLATRUNCULI_NNUE_NETWORK="$(NNUE_NETWORK)" -DLATRUNCULI_NNUE_NETWORK_SHA256=$(NNUE_SHA256)
	cmake --build build --target latrunculi --parallel 2
	cmake -E copy build/latrunculi $(EXE)

clean:
	cmake -E rm -rf build $(EXE)
