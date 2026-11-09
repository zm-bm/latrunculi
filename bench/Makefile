EXE ?= latrunculi
CXX ?= c++
TUNE ?= ON

.PHONY: all clean

all:
	cmake -S .. -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=$(CXX) -DLATRUNCULI_TUNING=$(TUNE)
	cmake --build build --target latrunculi --parallel
	cmake -E copy build/latrunculi $(EXE)

clean:
	cmake -E rm -rf build $(EXE)
