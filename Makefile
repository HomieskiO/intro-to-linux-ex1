# Compiler settings (overrideable from CLI)
CXX            ?= g++
CXXFLAGS       ?= -std=c++17 -fPIC -I$(SRCDIR)
LDFLAGS_SHARED ?= -shared

# Directories
SRCDIR   := src
BUILDDIR := build
BINDIR   := bin

# Library settings
LIB_NAME := blockchain_utils
LIB_SO   := $(BUILDDIR)/lib$(LIB_NAME).so
LIB_OBJS := $(BUILDDIR)/$(LIB_NAME).o

# Gather all .cpp files
SRC_CPP   := $(wildcard $(SRCDIR)/*.cpp)
# Exclude the library implementation from executables
EXE_SRCS  := $(filter-out $(SRCDIR)/$(LIB_NAME).cpp,$(SRC_CPP))
# Derive executable names (basename of each .cpp)
EXES      := $(patsubst $(SRCDIR)/%.cpp,%,$(EXE_SRCS))
EXES_BIN  := $(addprefix $(BINDIR)/,$(EXES))

# Default target
.PHONY: all
all: dirs $(LIB_SO) $(EXES_BIN)
	@echo
	@echo "Build ended successfully!"

# Create output dirs
.PHONY: dirs
dirs:
	@mkdir -p $(BUILDDIR) $(BINDIR)

# Build the shared library
$(LIB_SO): $(LIB_OBJS)
	$(CXX) $(LDFLAGS_SHARED) -o $@ $^

# Compile library object
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp $(SRCDIR)/%.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Build each executable, linking against the shared lib
$(BINDIR)/%: $(SRCDIR)/%.cpp $(LIB_SO)
	$(CXX) $(CXXFLAGS) -o $@ $< \
	  -L$(BUILDDIR) -l$(LIB_NAME) \
	  -Wl,-rpath,'$$ORIGIN/../build'

# Run the interactive shell
.PHONY: run
run: $(BINDIR)/bitcoinShell
	@$(BINDIR)/bitcoinShell

# Clean everything
.PHONY: clean
clean:
	@rm -rf $(BUILDDIR) $(BINDIR)
