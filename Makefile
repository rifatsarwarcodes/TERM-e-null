# OV_C build
#
#   make          build the ovc program
#   make test     build and run the unit tests
#   make clean    remove build output
#
# Works with g++ or clang++ on Linux/macOS and with MinGW-w64 (MSYS2) on
# Windows. On a plain Windows command prompt, use build.bat instead.

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -Isrc
BUILD    := build

ifeq ($(OS),Windows_NT)
  EXE := .exe
else
  EXE :=
endif

LIB_SRCS  := $(foreach dir,$(wildcard src/*/),$(wildcard $(dir)*.cpp))
LIB_OBJS  := $(patsubst %.cpp,$(BUILD)/%.o,$(LIB_SRCS))
APP_OBJ   := $(BUILD)/src/main.o
TEST_SRCS := $(wildcard tests/*.cpp)
TEST_OBJS := $(patsubst %.cpp,$(BUILD)/%.o,$(TEST_SRCS))

PROGRAM := $(BUILD)/ovc$(EXE)
TESTS   := $(BUILD)/ovc_tests$(EXE)

.PHONY: all test clean

all: $(PROGRAM)

$(PROGRAM): $(LIB_OBJS) $(APP_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

test: $(TESTS)
	./$(TESTS)

$(TESTS): $(LIB_OBJS) $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

clean:
	rm -rf $(BUILD)

-include $(LIB_OBJS:.o=.d) $(TEST_OBJS:.o=.d) $(APP_OBJ:.o=.d)
