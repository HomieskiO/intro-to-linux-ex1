# Compiler settings
CXX           := g++
CXXFLAGS      := -std=c++17 -fPIC -Isrc
LDFLAGS_SHARED:= -shared

# Directories
SRCDIR        := src
BUILDDIR      := build
BINDIR        := bin

# Library
LIBNAME       := libblockchain_utils.so
LIBOBJ        := $(BUILDDIR)/blockchain_utils.o

# Executables
EXES          := printDatabase blockFinder refreshDatabase bitcoinShell
EXES_BIN      := $(addprefix $(BINDIR)/,$(EXES))

# Default target
all: dirs $(BUILDDIR)/$(LIBNAME) $(EXES_BIN)
	@echo
	@echo "Build ended successfully!"

# Create directories if missing
dirs:
	@mkdir -p $(BUILDDIR) $(BINDIR)

# Build the shared library
$(BUILDDIR)/$(LIBNAME): $(LIBOBJ)
	$(CXX) $(LDFLAGS_SHARED) -o $@ $^

# Compile library object
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp $(SRCDIR)/%.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Build each executable, linking against the shared lib
$(BINDIR)/%: $(SRCDIR)/%.cpp $(BUILDDIR)/$(LIBNAME)
	$(CXX) $(CXXFLAGS) -o $@ $< \
	  -L$(BUILDDIR) -lblockchain_utils \
	  -Wl,-rpath,'$$ORIGIN/../build'

# Run the interactive shell
.PHONY: run
run: $(BINDIR)/bitcoinShell
	@$(BINDIR)/bitcoinShell

# Clean out everything in build/ and bin/
.PHONY: clean
clean:
	rm -rf $(BUILDDIR) $(BINDIR)