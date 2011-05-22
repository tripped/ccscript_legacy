#
# Makefile for CCScript tests project
#

CXX = g++
CXXFLAGS = -c -Wall -O3 
OBJDIR = obj
BINDIR = bin
OUTFILE = tests
SRCDIR = src
SOURCES = $(SRCDIR)/main.cpp $(SRCDIR)/test.cpp
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)
SEP = /

# Assume that if we don't have $(HOME), we're building on Windows
ifndef HOME
  OUTFILE = tests.exe
  RM = del 2>NUL
  SEP = $(shell echo \)
endif


#
# Targets
#
all: tests

tests: mkdirs $(OBJECTS) 
	$(CXX) $(OBJECTS) -o $(BINDIR)/$@


# Ensure that bin and obj directories exist
mkdirs:
  ifeq "$(wildcard $(OBJDIR) )" ""
	@-mkdir $(OBJDIR)
  endif
  ifeq "$(wildcard $(BINDIR) )" ""
	@-mkdir $(BINDIR)
  endif


$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

#
# Dependencies
#
$(OBJDIR)/test.o: src/test.h
$(OBJDIR)/main.o: src/test.h src/consolecolor.h


.PHONY: clean mkdirs

clean:
	-$(RM) $(OBJDIR)$(SEP)*.o $(BINDIR)$(SEP)$(OUTFILE)