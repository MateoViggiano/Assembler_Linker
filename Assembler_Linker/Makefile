CXX := clang++
CXXFLAGS := -std=c++20 -Wall -Wextra -pedantic -fsanitize=address
SHELL := powershell.exe
BINDIR := bin
TARGETS := $(BINDIR)/vm.exe $(BINDIR)/vmasm.exe $(BINDIR)/vmlink.exe
BINARYS := $(wildcard $(BINDIR)/*.exe $(BINDIR)/*.pdb $(BINDIR)/*.vmo $(BINDIR)/*.vmexe)
ASMLIBS := $(BINDIR)/fact.vmo $(BINDIR)/fact_rec.vmo
.PHONY: all exec cleanexec clean asmlibs asmall prog1 prog2 prog1d prog2d

all: exec $(BINDIR)/main.vmexe $(BINDIR)/main2.vmexe

exec: $(TARGETS)
$(BINDIR):
	if (-not (Test-Path -LiteralPath "$(BINDIR)")) { New-Item -ItemType Directory -Path "$(BINDIR)" | Out-Null }
$(BINDIR)/vm.exe: vm.cpp vmdbg.hpp | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -o $@
$(BINDIR)/vmasm.exe: vmasm.cpp Comment_and_spaces_deleter.cpp Tokenizer.cpp InstructionMaker2.0.cpp | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -o $@
$(BINDIR)/vmlink.exe: vmlink.cpp | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) $< -o $@

clean:
	@if ("$(BINARYS)" -ne "") { foreach ($$f in "$(BINARYS)".Split(" ")) { if (Test-Path $$f) { Remove-Item $$f -Force } } }

%.vmo: %.asm | $(BINDIR)/vmasm.exe
	$(BINDIR)/vmasm.exe $<

%.vmexe: %.vmo | $(BINDIR)/vmlink.exe
	$(BINDIR)/vmlink.exe $* $<


asmlibs: $(ASMLIBS)
asmall: $(BINDIR)/main.vmo $(BINDIR)/main2.vmo asmlibs
 
$(BINDIR)/main.vmexe: $(BINDIR)/main.vmo $(ASMLIBS) | $(BINDIR)/vmlink.exe
	$(BINDIR)/vmlink.exe $(BINDIR)/main $^

$(BINDIR)/main2.vmexe: $(BINDIR)/main2.vmo $(ASMLIBS) | $(BINDIR)/vmlink.exe
	$(BINDIR)/vmlink.exe $(BINDIR)/main2 $^

prog1: $(BINDIR)/main.vmexe | $(BINDIR)/vm.exe
	$(BINDIR)/vm.exe $(BINDIR)/main.vmexe

prog2: $(BINDIR)/main.vmexe | $(BINDIR)/vm.exe
	$(BINDIR)/vm.exe $(BINDIR)/main2.vmexe

prog1d: $(BINDIR)/main.vmexe | $(BINDIR)/vm.exe
	$(BINDIR)/vm.exe $(BINDIR)/main.vmexe -debug

prog2d: $(BINDIR)/main.vmexe | $(BINDIR)/vm.exe
	$(BINDIR)/vm.exe $(BINDIR)/main2.vmexe -debug