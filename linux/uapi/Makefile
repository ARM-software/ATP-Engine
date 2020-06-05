src := $(wildcard *.c)
obj := $(src:.c=.o)
dep := $(src:.c=.d)
out := libatp

# Path to exported kernel user API header files
# https://www.kernel.org/doc/html/latest/kbuild/headers_install.html
KERNEL_HDR_PATH ?= /usr/include

ARFLAGS  := Ucrs
CPPFLAGS := -I../../ -I$(KERNEL_HDR_PATH) -MMD -MP
CFLAGS   := -std=c11 -Wall -Wextra -Werror -pedantic
ifeq ($(DEBUG), 1)
CFLAGS   += -O0 -g3
else
CFLAGS   += -O3
endif

PREFIX := /usr/local

.PHONY: all static
all: $(out).so
$(out).so: CFLAGS += -fPIC
$(out).so: LDFLAGS += -shared
$(out).so: $(obj)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
static: $(out).a($(obj))

.PHONY: install install_static install_common
install: all install_common
	cp $(out).so $(DESTDIR)$(PREFIX)/lib
install_static: static install_common
	cp $(out).a $(DESTDIR)$(PREFIX)/lib
install_common:
	mkdir -p $(DESTDIR)$(PREFIX)/lib
	mkdir -p $(DESTDIR)$(PREFIX)/include
	cp atp.h $(DESTDIR)$(PREFIX)/include

.PHONY: uninstall uninstall_static uninstall_common
uninstall: uninstall_common
	$(RM) $(DESTDIR)$(PREFIX)/lib/$(out).so
uninstall_static:
	$(RM) $(DESTDIR)$(PREFIX)/lib/$(out).a
uninstall_common:
	$(RM) $(DESTDIR)$(PREFIX)/include/atp.h

.PHONY: clean
clean:
	$(RM) $(obj) $(dep) $(out).so $(out).a

-include $(dep)
