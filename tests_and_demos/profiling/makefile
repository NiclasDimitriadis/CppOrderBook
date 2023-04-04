CXX = g++
COMPILER_FLAGS = -std=c++23 -O3
INCLUDE_DIRS = -I ../../src/production -I ../../src/testing
UNITS = Profiling_multithreaded Profiling_CPU_aff Profiling_single_threaded
OBJ_FILES = $(addprefix build/,$(addsuffix .o,$(UNITS)))
EXECUTABLES = $(addprefix build/,$(UNITS))


all: $(OBJ_FILES) $(EXECUTABLES)

build/%.o: ../../src/testing/%.cpp
	$(CXX) $(COMPILER_FLAGS) $(INCLUDE_DIRS) -c $< -o $@

build/%: build/%.o
	$(CXX) $(COMPILER_FLAGS) $(INCLUDE_DIRS) $< -o $@	

.PHONY: clean all

clean:
	rm -rf build/*.o $(EXECUTABLES)