#
# Crappy makefile for ccscript compiler (ccc)
#
CXX = g++
CXXFLAGS = -c -Wall -O3 
OBJDIR = obj
BINDIR = bin
OUTFILE = ccc
SOURCES = ccc.cpp compiler.cpp module.cpp bytechunk.cpp lexer.cpp parser.cpp ast.cpp \
          stringparser.cpp symboltable.cpp table.cpp value.cpp anchor.cpp
OBJECTS = $(SOURCES:%.cpp=$(OBJDIR)/%.o)
INSTALL_DIR = /usr/local

#
# Miscelaneous environment info
#
CP = cp
SEP = /
TESTCMD = tests/bin/tests $(BINDIR)/ccc tests/tests.txt
RUNTESTS = ./runtests


#
# Assume that if we don't have $(HOME), we're building on Windows;
# We'll have to tweak the environment variables a bit.
#
ifndef HOME
  WINDOWS = very yes
  CP = copy 1>NUL
  OUTFILE = ccc.exe
  TESTCMD = @tests\bin\tests.exe $(BINDIR)\ccc.exe tests\tests.txt
  RUNTESTS = runtests.bat
  RM = del 2>NUL
# This is apparently necessary to assign the value '\' to a variable.
  SEP = $(shell echo \)
endif


#
# Targets
#
	
all: ccc tests runtests


# Builds the compiler
ccc: mkdirs libsdir $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(BINDIR)/$@


# Ensure that bin and obj directories exist
mkdirs:
  ifeq "$(wildcard $(OBJDIR) )" ""
	@-mkdir $(OBJDIR)
  endif
  ifeq "$(wildcard $(BINDIR) )" ""
	@-mkdir $(BINDIR)
  endif


# Copy the standard libraries to bin/lib
libsdir:
	@echo Copying std.ccs and stdarg.ccs to library directory...
  ifeq "$(wildcard $(BINDIR)/lib )" ""
	@-mkdir $(BINDIR)$(SEP)lib
  endif
	@$(CP) std.ccs $(BINDIR)$(SEP)lib
	@$(CP) stdarg.ccs $(BINDIR)$(SEP)lib

# Builds test framework
tests:
	@echo Building test framework ...
	$(MAKE) -C tests
	@echo Creating $(RUNTESTS) script...
	@echo $(TESTCMD) > $(RUNTESTS)
  ifndef WINDOWS
	@chmod 744 $(RUNTESTS)
  endif


# Runes regression tests
runtests: tests
	@echo Running tests...
	@$(RUNTESTS)


$(OBJDIR)/%.o: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@



#
# Object dependencies
#
$(OBJDIR)/ccc.o:			module.h
$(OBJDIR)/compiler.o:		compiler.h module.h ast.h bytechunk.h symboltable.h exception.h
$(OBJDIR)/module.o:			module.h compiler.h ast.h lexer.h parser.h symboltable.h bytechunk.h exception.h
$(OBJDIR)/bytechunk.o:		bytechunk.h ast.h
$(OBJDIR)/lexer.o: 			lexer.h
$(OBJDIR)/parser.o: 		parser.h lexer.h ast.h
$(OBJDIR)/ast.o: 			ast.h symboltable.h bytechunk.h module.h stringparser.h exception.h
$(OBJDIR)/stringparser.o:	stringparser.h ast.h parser.h module.h bytechunk.h
$(OBJDIR)/symboltable.o: 	symboltable.h ast.h
$(OBJDIR)/anchor.o:			anchor.h
$(OBJDIR)/value.o:			value.h table.h function.h string.h
$(OBJDIR)/table.o:			table.h


.PHONY: clean tests mkdirs libsdir runtests

clean:
	-$(RM) $(OBJDIR)$(SEP)*.o $(BINDIR)$(SEP)$(OUTFILE)
	-$(MAKE) -C tests clean
	

