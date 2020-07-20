CC := g++
CXXFLAGS := -std=c++14
ifeq ($(DEBUG),1)
  CXXFLAGS += -g -DDEBUG
else
  CXXFLAGS += -O2
endif

program := program
cpp-files := $(sort $(wildcard *.cpp))
object-files := $(cpp-files:.cpp=.o)

# Function definitions

get-object-dependencies = $(shell { \
  $(CXX) -MG -MT $(1:.cpp=.o) -MM $1 || echo ' $$(error E =' $$? ')'; \
  } | sed 's:\\$$::g' \
)

compile-command = $(CXX) $(CXXFLAGS) -o $2 -c $1
define compile-to-object =
  $(call get-object-dependencies, $1)
	$(CXX) $(CXXFLAGS) -o $$@ -c $$<
endef

# Targets and rules

all: $(program)

$(foreach cpp, $(cpp-files), \
  $(eval $(call compile-to-object, $(cpp))) \
)

$(program): $(object-files)
	$(CC) $(CXXFLAGS) $^ -o $@

clean:
	rm -f *.o $(program)

.PHONY: all clean
