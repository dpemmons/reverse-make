include ${CURDIR}/common.mk

.DEFAULT_GOAL := all

BASE_BUILD_DIR := ${CURDIR}/build/${BUILD}

###############################################################################
# reverse-make binary rules
REVERSE_MAKE_SRC_DIRS := ./reverse-make
REVERSE_MAKE_BUILD_DIR := $(BASE_BUILD_DIR)/reverse-make-build
REVERSE_MAKE_EXEC := $(BASE_BUILD_DIR)/reverse-make

REVERSE_MAKE_SRCS := $(shell find $(REVERSE_MAKE_SRC_DIRS) -name '*.cpp' -or -name '*.c' -or -name '*.s')
REVERSE_MAKE_OBJS := $(REVERSE_MAKE_SRCS:%=$(REVERSE_MAKE_BUILD_DIR)/%.o)
REVERSE_MAKE_DEPS := $(REVERSE_MAKE_OBJS:.o=.d)
REVERSE_MAKE_INC_DIRS := $(shell find $(REVERSE_MAKE_SRC_DIRS) -type d) . $(CLI_LIB_HEADERS) $(FMT_LIB_HEADERS) $(ANTLR4_LIB_HEADERS)
REVERSE_MAKE_INC_FLAGS := $(addprefix -I,$(REVERSE_MAKE_INC_DIRS))
REVERSE_MAKE_CPPFLAGS := $(GLOBAL_CPPFLAGS) $(REVERSE_MAKE_INC_FLAGS) $(LIB_INC_FLAGS) #-Werror -Wall -Wextra 

$(REVERSE_MAKE_BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(REVERSE_MAKE_CPPFLAGS) $(CFLAGS) -c $< -o $@

$(REVERSE_MAKE_BUILD_DIR)/%.cpp.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(REVERSE_MAKE_CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(REVERSE_MAKE_EXEC): $(REVERSE_MAKE_OBJS) $(LIBREVERSE_MAKE_STATIC_OBJ)
	$(CXX) $(REVERSE_MAKE_OBJS) $(LIBREVERSE_MAKE_STATIC_OBJ) -o $@ $(LDFLAGS)
#
###############################################################################

.PHONY: clean
clean:
	rm -rf $(BASE_BUILD_DIR)/libreverse-make $(BASE_BUILD_DIR)/reverse-make $(BASE_BUILD_DIR)/test

.PHONY: reverse-make-clean
reverse-make-clean:
	rm -rf $(BASE_BUILD_DIR)/reverse-make

.PHONY: all-clean
all-clean:
	rm -rf build

reverse-make: $(REVERSE_MAKE_EXEC)

all: reverse-make

# Include the .d makefiles. The - at the front suppresses the errors of missing
# Makefiles. Initially, all the .d files will be missing, and we don't want those
# errors to show up.
-include $(LIB_DEPS)