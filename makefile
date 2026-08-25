# 2026/08/12 BG makefile cleaned up for v1.0

.DEFAULT_GOAL := S2_Decoder

CXX      := g++
CXXFLAGS := -g1 -O2 -std=c++20 -MMD -MP
LDLIBS   :=

SRC := src
OBJ := $(SRC)/.objectfiles

# Sources used by S2_Decoder
SOURCES := $(wildcard $(SRC)/*/*.cpp) $(SRC)/main.cpp

# Exclude L1 code from L0/SBD S2_Decoder
EXCLUDE := $(SRC)/L1/L0toL1_S2.cpp $(SRC)/L1/SBD_S2.cpp
SOURCES := $(filter-out $(EXCLUDE),$(SOURCES))

# Map src/subdir/source.cpp -> src/.objectfiles/source.o
OFILES := $(addprefix $(OBJ)/,$(notdir $(SOURCES:.cpp=.o)))
DFILES := $(OFILES:.o=.d)

S2_Decoder: $(OFILES)
	$(CXX) $(CXXFLAGS) $^ -o $@
#	$(CXX) $(CXXFLAGS) $^ $(LDLIBS) -o $@

#L0toL1: src/L1/L0toL1_S2.cpp
#	$(CXX) $(CXXFLAGS) -MF $(OBJ)/L1.d $^ $(LDLIBS) -o $@

#SBD:    src/L1/SBD_S2.cpp
#	$(CXX) $(CXXFLAGS) -MF $(OBJ)/L1.d $^ $(LDLIBS) -o $@

#L0toL1: src/L1/L0toL1_S2.cpp
#	g++ -g -std=c++20 src/L1/L0toL1_S2.cpp -o L0toL1

#SBD:    src/L1/SBD_S2.cpp
#	g++ -g -std=c++20 src/L1/SBD_S2.cpp -o SBD

# Generate one compile rule for each source file
define COMPILE_RULE
$(OBJ)/$(notdir $(1:.cpp=.o)): $(1) | $(OBJ)
	$(CXX) $(CXXFLAGS) -c $$< -o $$@
endef

$(foreach file,$(SOURCES),$(eval $(call COMPILE_RULE,$(file))))

# Create directory structure on first make
$(OBJ):
	mkdir -p $@ log data incoming

-include $(DFILES)

.PHONY: clean
clean:
	rm -f S2_Decoder L0toL1 SBD $(OFILES) $(DFILES)


