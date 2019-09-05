# Antonius makefile

CC := g++
SRCEXT := cpp
SRCDIR := src
BUILDDIR := build
TESTDIR := test

TARGET := Antonius
TESTTARGET := AntoniusTest

SOURCES := $(shell find $(SRCDIR) -type f -name *.$(SRCEXT))
OBJECTS := $(patsubst $(SRCDIR)/%,$(BUILDDIR)/%,$(SOURCES:.$(SRCEXT)=.o))

TESTSOURCES := $(shell find $(TESTDIR) -type f -name *.$(SRCEXT))
TESTOBJECTS := $(patsubst $(TESTDIR)/%,$(BUILDDIR)/%,$(TESTSOURCES:.$(SRCEXT)=.o))
TESTOBJECTS += $(filter-out $(BUILDDIR)/main.o, $(OBJECTS))

CFLAGS := -O3 -g3 -ggdb -std=c++17 -Wall -Wextra -Wsign-conversion
# CFLAGS := -g3 -ggdb -fkeep-inline-functions -std=c++17 -Wall -Wextra -Wsign-conversion
LIB :=
INC := -I include

$(TARGET): $(OBJECTS)
	@echo " Linking..."
	@echo " $(CC) $^ -o $(TARGET) $(LIB)"; $(CC) $^ -o $(TARGET) $(LIB)

$(BUILDDIR)/%.o: $(SRCDIR)/%.$(SRCEXT)
	@mkdir -p $(BUILDDIR)
	@echo " $(CC) $(CFLAGS) $(INC) -c -o $@ $<"; $(CC) $(CFLAGS) $(INC) -c -o $@ $<

test: $(TESTOBJECTS)
	@echo " Linking..."
	@echo "$(CC) $^ -o $(TESTTARGET) $(LIB)"; $(CC) $^ -o $(TESTTARGET) $(LIB)

$(BUILDDIR)/%.o: $(TESTDIR)/%.$(SRCEXT)
	@mkdir -p $(BUILDDIR)
	@echo " $(CC) $(CFLAGS) $(INC) -c -o $@ $<"; $(CC) $(CFLAGS) $(INC) -c -o $@ $<

clean:
	@echo " Cleaning...";
	@echo " $(RM) -r $(BUILDDIR) $(TARGET) $(TESTTARGET)"; $(RM) -r $(BUILDDIR) $(TARGET) $(TESTTARGET)

.PHONY: clean test
