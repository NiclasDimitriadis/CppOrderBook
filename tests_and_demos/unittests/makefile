CXX = g++
COMPILER_FLAGS = -std=c++23 -O3 -ggdb
INCLUDE_DIRS = -I ../../src/production -I ../../src/testing -I ../../src/testing/doctest
UNITS = Unittests_Auxil Unittests_AtomicGuard Unittests_FileToTuples Unittests_FIXmockSocket\
	 Unittests_FIXsocketHandler Unittests_OrderBook Unittests_SeqLockElement Unittests_SeqLockQueue
OBJ_FILES = $(addprefix build/,$(addsuffix .o,$(UNITS)))
EXECUTABLES = $(addprefix build/,$(UNITS))


all: $(OBJ_FILES) $(EXECUTABLES)

build/%.o: ../../src/testing/%.cpp
	$(CXX) $(COMPILER_FLAGS) $(INCLUDE_DIRS) -c $< -o $@

build/%: build/%.o
	$(CXX) $(COMPILER_FLAGS) $(INCLUDE_DIRS) $< -o $@	


.PHONY: clean all

clean:
	rm -rf  build/Unittests_*