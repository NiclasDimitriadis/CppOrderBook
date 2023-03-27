CXX = g++
UNITTEST_FLAGS = -std=c++23 -g
FLAGS = -std=c++23 -O3
INCLUDE_DIRS = -I ./production -I ./testing/doctest -I ./testing
TARGET_DIR = ../build
TEST_TARGET_DIR = ../tests

$(TEST_TARGET_DIR)/%.o : ./production/%.cpp
	$(CXX) $(UNITTEST_FLAGS) $(INCLUDE_DIRS) -c -o $@ $<

clean:
	rm -rf $(TARGET_DIR)/*.o $(TEST_TARGET_DIR)/*.o $(TEST_TARGET_DIR)/Unittests