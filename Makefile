.DEFAULT_GOAL := all

CC = gcc
CXX = g++
NNUE ?= 1
SIMD ?= scalar
BUILD ?= build/$(NNUE)-$(SIMD)
CPPFLAGS += -Iinc -D_GNU_SOURCE
CFLAGS ?= -O3 -std=c11 -Wall -Wextra
CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra
CXXFLAGS += -ffunction-sections -fdata-sections -DNNUE_EMBEDDING_OFF -DIS_64BIT -DNDEBUG
ifneq ($(findstring clang,$(CXX)),)
CXXFLAGS += -fconstexpr-steps=500000000
else
CXXFLAGS += -fconstexpr-ops-limit=500000000
endif
ifeq ($(OS),Windows_NT)
LDFLAGS += -Wl,--gc-sections
else
ifeq ($(shell uname -s),Darwin)
LDFLAGS += -Wl,-dead_strip
else
LDFLAGS += -Wl,--gc-sections
endif
endif
ifeq ($(SIMD),avx2)
CXXFLAGS += -mavx2 -DUSE_AVX2 -DUSE_SSE2 -DUSE_SSSE3 -DUSE_SSE41
endif
ifeq ($(SIMD),neon)
CXXFLAGS += -DUSE_NEON
endif
ifeq ($(OS),Windows_NT)
EXE = .exe
PYTHON ?= python
else
PYTHON ?= python3
endif
ENGINE_SRC = $(wildcard src/*.c)
CORE_SRC = $(filter-out src/main.c src/uci.c src/game_loop.c src/puzzle.c src/opening_book.c,$(ENGINE_SRC))
SF = nnue/stockfish/src
SF_SRC = $(SF)/attacks.cpp $(SF)/position.cpp $(SF)/misc.cpp $(SF)/memory.cpp \
 $(SF)/nnue/network.cpp $(SF)/nnue/nnue_accumulator.cpp \
 $(wildcard $(SF)/nnue/features/*.cpp)
ifeq ($(NNUE),1)
CPPFLAGS += -DCCE_NNUE -DCCE_NNUE_ONLY
NNUE_OBJ = $(patsubst %.cpp,$(BUILD)/%.o,$(SF_SRC) nnue/cce_nnue.cpp nnue/sha256.cpp) $(BUILD)/nnue/network_file.o
LINK = $(CXX)
else
LINK = $(CC)
endif
ENGINE_OBJ = $(patsubst %.c,$(BUILD)/%.o,$(ENGINE_SRC))
CORE_OBJ = $(patsubst %.c,$(BUILD)/%.o,$(CORE_SRC))
.PHONY: all clean diagnostics nnue-test FORCE
FORCE:
all: cce_engine$(EXE)
cce_engine$(EXE): $(ENGINE_OBJ) $(NNUE_OBJ) FORCE
	$(LINK) $(filter-out FORCE,$^) $(LDFLAGS) -o $@
diagnostics: engine_diagnostics$(EXE)
engine_diagnostics$(EXE): $(BUILD)/tests/engine_diagnostics.o $(CORE_OBJ) $(NNUE_OBJ) FORCE
	$(LINK) $(filter-out FORCE,$^) $(LDFLAGS) -o $@
nnue-test: nnue_benchmark$(EXE)
nnue_benchmark$(EXE): $(BUILD)/tests/nnue_benchmark.o $(CORE_OBJ) $(NNUE_OBJ) FORCE
	$(LINK) $(filter-out FORCE,$^) $(LDFLAGS) -o $@
$(BUILD)/%.o: %.c Makefile
	@$(PYTHON) -c "from pathlib import Path; Path(r'$(dir $@)').mkdir(parents=True, exist_ok=True)"
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@
$(BUILD)/%.o: %.cpp Makefile
	@$(PYTHON) -c "from pathlib import Path; Path(r'$(dir $@)').mkdir(parents=True, exist_ok=True)"
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@
clean:
	$(PYTHON) -c "import shutil; from pathlib import Path; shutil.rmtree(r'$(BUILD)', ignore_errors=True); [Path(n).unlink(missing_ok=True) for n in ('cce_engine$(EXE)', 'engine_diagnostics$(EXE)', 'nnue_benchmark$(EXE)')]"
-include $(wildcard $(BUILD)/src/*.d $(BUILD)/tests/*.d $(BUILD)/nnue/*.d $(BUILD)/nnue/stockfish/src/*.d $(BUILD)/nnue/stockfish/src/nnue/*.d $(BUILD)/nnue/stockfish/src/nnue/features/*.d)
