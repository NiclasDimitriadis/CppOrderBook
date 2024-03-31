CXX = g++-13
COMPILER_FLAGS = -std=c++23 -O3 -ggdb
INCLUDE_DIRS = -I ../../src/production -I ../../src/testing
UNITS = Unittests_Auxil Unittests_AtomicGuard Unittests_FileToTuples Unittests_FIXmockSocket\
	 Unittests_FIXsocketHandler Unittests_OrderBook Unittests_SeqLockElement Unittests_SeqLockQueue
EXECUTABLES = $(addprefix build/,$(UNITS))
OBJ_FILES = $(addsuffix .o,$(EXECUTABLES))


all: $(OBJ_FILES) $(EXECUTABLES)

build/%.o: ../../src/testing/%.cpp
	$(CXX) $(COMPILER_FLAGS) $(INCLUDE_DIRS) -c $< -o $@

build/%: build/%.o
	$(CXX) $(COMPILER_FLAGS) $(INCLUDE_DIRS) $< -o $@	


.PHONY: clean all

clean:
	rm -rf  build/Unittests_*
