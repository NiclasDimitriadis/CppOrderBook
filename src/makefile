CXX = g++
UNITTEST_FLAGS = -std=c++20 -I.. -I../doctest
FLAGS = -std=c++20 -O3
TEST_OBJECTS = AtomicFlagGuard.o

Unittests: Unittests.o $(TEST_OBJECTS)
	$(CXX) $(UNITTEST_FLAGS) $^ -o $@

Unittests.o: ../Unittests.cpp $(TEST_OBJECTS)
	$(CXX) -c $(UNITTEST_FLAGS) $< -o $@

AtomicFlagGuard.o:  ../AtomicFlagGuard.cpp
	$(CXX) -c $(UNITTEST_FLAGS) $< -o $@

clean:
	rm -rf *.o Unittests