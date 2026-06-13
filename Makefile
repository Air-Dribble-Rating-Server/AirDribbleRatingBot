CXX      := g++

# ─── Compilation flags ─────────────────────────────────────────
# Debug: warnings, symbols, native optimizations, no LTO
CXXFLAGS_DEBUG   := -std=c++23 -Wall -Wextra -Wpedantic -g -Og -march=native -MMD -MP
# Release: maximum optimization, portable, static linking, no warnings
CXXFLAGS_RELEASE := -std=c++23 -O3 -DNDEBUG -flto -MMD -MP
LDFLAGS_RELEASE  := -static -flto

# ─── Libraries (pkg-config) ────────────────────────────────────
DPP_CFLAGS  := $(shell pkg-config --cflags dpp)
DPP_LIBS    := $(shell pkg-config --libs dpp)
JSON_CFLAGS := $(shell pkg-config --cflags nlohmann_json 2>/dev/null || echo "")

# ─── Directories ───────────────────────────────────────────────
SRC_DIR          := src
BUILD_DIR        := build
BUILD_DIR_DEBUG  := $(BUILD_DIR)/debug
BUILD_DIR_RELEASE := $(BUILD_DIR)/release

# Recursively find all .cpp files in src/
SRCS := $(shell find $(SRC_DIR) -type f -name '*.cpp')

# Convert src/xxx.cpp to build/(debug|release)/xxx.o
OBJS_DEBUG   := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR_DEBUG)/%.o,$(SRCS))
OBJS_RELEASE := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR_RELEASE)/%.o,$(SRCS))

# Final binary name
TARGET := AirDribbleRatingBot

# ─── Main targets ──────────────────────────────────────────────
.PHONY: all clean run compile release compile-release debug-run

# Default (`make`): compile in debug mode and execute
debug-run: $(BUILD_DIR_DEBUG)/$(TARGET)
	@echo "▶ Running DEBUG build..."
	./$(BUILD_DIR_DEBUG)/$(TARGET)

# `make compile`: debug build only (no run)
compile: $(BUILD_DIR_DEBUG)/$(TARGET)
	@echo "✅ DEBUG build completed."

# `make release`: release build and execute
release: $(BUILD_DIR_RELEASE)/$(TARGET)
	@echo "▶ Running RELEASE build..."
	./$(BUILD_DIR_RELEASE)/$(TARGET)

# `make compile-release`: release build only (no run)
compile-release: $(BUILD_DIR_RELEASE)/$(TARGET)
	@echo "✅ RELEASE build completed."

# ─── Linking rules ─────────────────────────────────────────────
$(BUILD_DIR_DEBUG)/$(TARGET): $(OBJS_DEBUG)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_DEBUG) $^ $(DPP_LIBS) -o $@

$(BUILD_DIR_RELEASE)/$(TARGET): $(OBJS_RELEASE)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_RELEASE) $(LDFLAGS_RELEASE) $^ $(DPP_LIBS) -o $@

# ─── Object compilation rules (fixed) ──────────────────────────
# Debug objects
$(BUILD_DIR_DEBUG)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS_DEBUG) $(DPP_CFLAGS) $(JSON_CFLAGS) -c $< -o $@

# Release objects
$(BUILD_DIR_RELEASE)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS_RELEASE) $(DPP_CFLAGS) $(JSON_CFLAGS) -c $< -o $@

# ─── Clean ─────────────────────────────────────────────────────
clean:
	rm -rf $(BUILD_DIR)
	@echo "🧹 Build directories removed."

# ─── Auto-dependency inclusion ─────────────────────────────────
-include $(OBJS_DEBUG:.o=.d)
-include $(OBJS_RELEASE:.o=.d)

# ─── Keep `run` as an alias for `debug-run` (backwards compatibility) ──
run: debug-run