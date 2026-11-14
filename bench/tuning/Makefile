EXE ?= latrunculi
CXX ?= c++

.PHONY: all clean
all:
	$(MAKE) -C .. TUNING=ON CXX="$(CXX)" EXE="$(abspath $(EXE))"
clean:
	$(MAKE) -C .. clean EXE="$(abspath $(EXE))"
