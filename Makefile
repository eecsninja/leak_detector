CXX ?= g++

CXXFLAGS = -g -std=c++11 -I.

SOURCES = hooks.cc leak_detector.cc leak_analyzer.cc leak_detector_impl.cc \
	  ranked_list.cc leak_detector_value_type.cc spin_lock_wrapper.cc \
	  call_stack_table.cc custom_allocator.cc  call_stack_manager.cc \
	  base/hash.cc base/low_level_alloc.cc compact_address_map.cc main.cc
TARGET = leak
OBJECTS = $(SOURCES:.cc=.o)
HEADERS = *.h */*.h

all: leak

leak: $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o leak

.cc.o: $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) $(TARGET) *.o
