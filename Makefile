# SPDX-License-Identifier: BSD-3-Clause-Clear
#
# Copyright (c) 2016-2019 ARM Limited
# All rights reserved
# Authors: Matteo Andreozzi <matteo.andreozzi@arm.com>

# ATP Engine standalone Makefile
# it is required that you register or export in you library path both google protocol 
# buffer and cppunit libraries. i.e.
#
# export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:<protobuf_path>/lib
# export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:<cppunit_path>/lib
#

CXX             := g++
PROTOC          := protoc
PROTOBUF_C_FLAGS:= $(shell pkg-config --cflags protobuf)
CPPUNIT_C_FLAGS := $(shell pkg-config --cflags cppunit)
PROTOBUF_L_FLAGS:= $(shell pkg-config --libs protobuf)
CPPUNIT_L_FLAGS	:= $(shell pkg-config --libs cppunit)
CXX_FLAGS       := $(PROTOBUF_C_FLAGS) -std=c++17 -Wall -Werror -Wextra -Wno-unused-parameter -Wno-unused-variable $(CPPUNIT_C_FLAGS) -fPIC $(EXTRA_CXX_FLAGS)
LD_FLAGS        := $(PROTOBUF_L_FLAGS) $(CPPUNIT_L_FLAGS) $(EXTRA_LD_FLAGS)
PROTO_SRC_DIR   := ./proto/
PROTO_SRC       := $(wildcard $(PROTO_SRC_DIR)tp*.proto)
PROTO_DIR       := ./proto/
LIB_CPP_FILES   := kronos.cc utilities.cc event.cc event_manager.cc fifo.cc logger.cc packet_desc.cc packet_tagger.cc \
           packet_tracer.cc random_generator.cc stats.cc \
    		   traffic_profile_desc.cc traffic_profile_manager.cc traffic_profile_master.cc traffic_profile_checker.cc \
    		   traffic_profile_slave.cc traffic_profile_delay.cc
LIB_H_FILES     := $(LIB_CPP_FILES:.cc=.hh) types.hh
LIB_OBJ_FILES   := $(LIB_CPP_FILES:.cc=.o)
TEST_CPP_FILES  := shell.cc test_atp.cc test.cc
TEST_H_FILES    := test_atp.hh shell.hh
TEST_OBJ_FILES  := $(TEST_CPP_FILES:.cc=.o)
CPP_FILES       := $(LIB_CPP_FILES) $(TEST_CPP_FILES)
H_FILES         := $(LIB_H_FILES) $(TEST_H_FILES)
PROTO_OBJ_FILES := $(addprefix $(PROTO_DIR), $(notdir $(PROTO_SRC:.proto=.pb.o)))
PROTO_CPP_FILES := $(PROTO_OBJ_FILES:.o=.cc)
PROTO_H_FILES   := $(PROTO_OBJ_FILES:.o=.h)

# install command (MKDIR and FILE) and install directory for the make install target
prefix		?= ./install
exec_prefix	?= $(prefix)
bindir		?= $(exec_prefix)/bin
libdir		?= $(exec_prefix)/lib
includedir	?= $(prefix)/include/arm/atp
INSTALL_EXEC	:= install -m 755
INSTALL_MKDIR	:= $(INSTALL_EXEC) -d
INSTALL_FILE	:= install -m 644

# binary name
BIN             := atpeng
STATIC_LIB	:= libatp.a
# log file name for debug_file target
LOG_FILE_NAME   := atp.log

# create protocol buffer directory if it does not exist
$(shell mkdir -p $(PROTO_DIR))

all: CXX_FLAGS += -O3
all: $(BIN) $(STATIC_LIB)

debug: CXX_FLAGS += -O0 -ggdb
debug: $(BIN)

debug_file: CXX_FLAGS += -DLOG_FILE="\"$(LOG_FILE_NAME)\""
debug_file: debug

.PHONY: clean cleanest install install-include install-include-proto install-lib

%.pb.cc %.pb.h: %.proto
	$(PROTOC) -I $(PROTO_SRC_DIR) --cpp_out=$(PROTO_DIR) $<

$(BIN): $(TEST_OBJ_FILES) $(STATIC_LIB)
	$(CXX) $^ $(LD_FLAGS) -o $@

$(STATIC_LIB): $(LIB_OBJ_FILES) $(PROTO_OBJ_FILES)
	ar rcs $(STATIC_LIB) $^

%.o: %.cc $(PROTO_H_FILES) $(H_FILES)
	$(CXX) $(CPPFLAGS) $(CXX_FLAGS) -c -o $@ $<

clean:
	@rm -rf $(PROTO_OBJ_FILES)
	@rm -rf $(PROTO_H_FILES)
	@rm -rf $(PROTO_CPP_FILES)
	@rm -rf *.o
	@rm -rf $(STATIC_LIB)

cleanest: clean
	@rm -rf $(BIN)

count:
	@echo "Source code lines:"
	@find . -name '*.cc' -o -name '*.hh'| xargs wc -l
	@echo "Google Protocol Buffer code lines:"
	@find $(PROTO_SRC_DIR) -name 'tp*.proto' | xargs wc -l

doc: $(H_FILES) $(CPP_FILES)
	@doxygen doxygen.cfg

install: install-include install-include-proto install-lib install-bin

install-include-proto: $(PROTO_H_FILES)
	$(INSTALL_MKDIR) $(includedir)/proto
	$(INSTALL_FILE) $^ $(includedir)/proto

install-include: $(LIB_H_FILES)
	$(INSTALL_MKDIR) $(includedir)
	$(INSTALL_FILE) $^ $(includedir)

install-lib: $(STATIC_LIB)
	$(INSTALL_MKDIR) $(libdir)
	$(INSTALL_FILE) $^ $(libdir)

install-bin: $(BIN)
	$(INSTALL_MKDIR) $(bindir)
	$(INSTALL_EXEC) $^ $(bindir)
