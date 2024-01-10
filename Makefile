TARGET ?= summa

GIT_VERSION ?= $(shell git describe --abbrev=4 --dirty --always --tags)
INSTALL ?= install -s
PREFIX ?= ${HOME}
BINDIR ?= $(PREFIX)/bin

CFLAGS += -DVERSION=$(GIT_VERSION)
LDFLAGS += -ly -ll

SRCS := $(wildcard *.c)
HDRS := $(wildcard *.h)
OBJS := $(SRCS:.c=.o) $(TARGET).o $(TARGET).yy.o

GENLH := $(TARGET).yy.h
GENLC := $(TARGET).yy.c
GENL := $(GENLC) $(GENLH)
GENB := $(TARGET).c $(TARGET).h

$(TARGET): $(OBJS)

$(OBJS): $(SRCS) $(HDRS) $(GENB)

$(GENL): $(TARGET).l
	flex $<

$(GENB): $(TARGET).y $(GENL)
	bison -Wcounterexamples -Wconflicts-sr --debug -d -o $@ $< 

.PHONY: all
all: $(TARGET)

.PHONY: install
install: $(TARGET)
	$(INSTALL) $< $(BINDIR)/

.PHONY: test
test: $(TARGET)
	@./$< < testdata/sample.txt

.PHONY: run
run: test

.PHONY: clean
clean:
	-rm -v $(TARGET) $(OBJS) $(GENL) $(GENB) 2>/dev/null
