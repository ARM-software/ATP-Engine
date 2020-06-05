src := $(wildcard *.cc)
obj := $(src:.cc=.o)
dep := $(src:.cc=.d)
out := test_atp

CPPFLAGS	:= -I../uapi -MMD -MP
CXXFLAGS	:= -std=c++11 -Wall -Wextra -Werror -pedantic
LDFLAGS		?= -L../uapi
LDLIBS		:= -latp -lcppunit
ifeq ($(DEBUG), 1)
CXXFLAGS	+= -O0 -g3
else
CXXFLAGS	+= -O3
endif

PREFIX := /usr/local

.PHONY: all
all: $(out).out
$(out).out: $(obj)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

.PHONY: install
install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp $(out).out $(DESTDIR)$(PREFIX)/bin

.PHONY: clean
clean:
	$(RM) $(obj) $(dep) $(out).out

-include $(dep)
