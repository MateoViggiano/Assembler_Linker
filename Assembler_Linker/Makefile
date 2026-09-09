CXX ?= g++
CXXFLAGS ?= -std=c++17

BINDIR := bin
TARGETS := $(BINDIR)/vm.exe $(BINDIR)/vmasm.exe $(BINDIR)/vmlink.exe

.PHONY: all clean

all: $(TARGETS)

$(BINDIR):
	if not exist "$(BINDIR)" mkdir "$(BINDIR)"

$(BINDIR)/vm.exe: vm.cpp vmdbg.hpp | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -o $@

$(BINDIR)/vmasm.exe: vmasm.cpp Comment_and_spaces_deleter.cpp Tokenizer.cpp InstructionMaker2.0.cpp | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -o $@

$(BINDIR)/vmlink.exe: vmlink.cpp | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -o $@

clean:
	if exist "$(BINDIR)\vm.exe" del /Q "$(BINDIR)\vm.exe"
	if exist "$(BINDIR)\vmasm.exe" del /Q "$(BINDIR)\vmasm.exe"
	if exist "$(BINDIR)\vmlink.exe" del /Q "$(BINDIR)\vmlink.exe"