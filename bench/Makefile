EXE ?= latrunculi
CXX ?= c++
TUNING ?= OFF

.PHONY: all clean

all:
	cmake -S .. -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=$(CXX) -DLATRUNCULI_TUNING=$(TUNING)
	cmake --build build --target latrunculi --parallel 2
	cmake -E copy build/latrunculi $(EXE)

clean:
	cmake -E rm -rf build $(EXE)
