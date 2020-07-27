CC := g++
CXXFLAGS := -Wall -std=c++17
ifeq ($(DEBUG),1)
  CXXFLAGS += -g -DDEBUG
else
  CXXFLAGS += -O2
endif

program := program
test := test
cpp-files := $(sort $(wildcard *.cpp))
test-cpp-files := $(sort $(wildcard unit_test/*.cpp))
object-files := $(cpp-files:.cpp=.o)
test-object-files := $(test-cpp-files:.cpp=.o)
test-object-files += $(filter-out main.o, $(object-files))
test-object-files += gtest/libgtest.a gtest/libgtest_main.a


# Function definitions

get-object-dependencies = $(shell { \
  $(CXX) -MG -MT $(1:.cpp=.o) -MM $1 || echo ' $$(error E =' $$? ')'; \
  } | sed 's:\\$$::g' \
)

define compile-to-object =
  $(call get-object-dependencies, $1)
	$(CXX) $(CXXFLAGS) -Igtest/include -o $$@ -c $$<
endef

# Targets and rules

all: $(program) $(test)

$(foreach cpp, $(cpp-files) $(test-cpp-files), \
  $(eval $(call compile-to-object, $(cpp))) \
)

$(program): $(object-files)
	$(CC) $(LDFLAGS) $^ -o $@

$(test): $(test-object-files)
	$(CC) -pthread $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o unit_test/*.o $(program) $(test)

.PHONY: all clean
