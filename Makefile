root_dir := $(realpath $(CURDIR))
srcs_dir := $(root_dir)/src
tests_dir := $(root_dir)/tests

build_dir := $(root_dir)/build
build_lib_dir := $(build_dir)/lib
build_tests_dir := $(build_dir)/tests

target_lib_static = $(build_dir)/lib/libcilk.a
target_lib_shared = $(build_dir)/lib/libcilk.so

test_srcs    := $(wildcard $(tests_dir)/test-*.c)
test_names   := $(subst $(tests_dir)/test-,,$(test_srcs:.c=))
test_targets := $(subst $(tests_dir),$(build_tests_dir),$(test_srcs:.c=))

# Decide whether the commands will be shown or not
verbose = FALSE

# Generate the GCC includes parameters by adding -I before each source folder
include_args = $(foreach dir,$(root_dir)/include $(root_dir)/include/cilk $(srcs_dir),$(addprefix -I, $(dir)))

# Add this list to VPATH, the place make will look for the source files
VPATH = $(srcs_dir)

# Create a list of *.c sources in DIRS
srcs = $(foreach dir,$(srcs_dir),$(wildcard $(dir)/*.c))
hdrs = $(foreach dir,$(root_dir)/include $(root_dir)/include/cilk,$(wildcard $(dir)/*.h))

# Define objects for all sources
objs := $(subst $(srcs_dir),$(build_dir),$(srcs:.c=.o))
obj_names := $(subst $(srcs_dir)/,,$(srcs:.c=))

ifeq ($(verbose),TRUE)
	hide =
else
	hide = @
endif

.PHONY: all
all: $(target_lib_static) $(target_lib_shared) tests

.PHONY: tests
tests: $(test_targets)

.PHONY: run-tests
run-tests: $(test_targets)
	$(hide)tools/run-tests.bash

$(target_lib_static): $(objs) | $(build_lib_dir)
	@echo "    Creating static archive $@"
	$(hide)$(AR) rcs $@ $(objs)

$(target_lib_shared): $(objs) | $(build_lib_dir)
	@echo "    Creating shared library $@"
	$(hide)$(CC) -shared -o $@ $(objs)

define generateObjRules
$$(build_dir)/$(1).o: $$(srcs_dir)/$(1).c $$(hdrs) | $$(build_dir)
	@echo "    Building object $$@"
	$$(hide)$$(CC) -g -c -fPIC -std=c11 $$(include_args) -o $$@ $$< -MMD
endef

define generateTestRules
$$(build_tests_dir)/test-$(1): $$(tests_dir)/test-$(1).c $$(hdrs) $$(target_lib_shared) | $$(build_tests_dir)
	@echo "    Building test $$@"
	$$(hide)$$(CC) -g -std=gnu11 -Wpedantic -fsanitize=thread $$(include_args) -o $$@ -lcilk -L$$(build_lib_dir) $$<
endef

$(foreach obj,$(obj_names),$(eval $(call generateObjRules,$(obj))))
$(foreach test,$(test_names),$(eval $(call generateTestRules,$(test))))

$(build_dir):
	$(hide)mkdir $@

$(build_lib_dir): | $(build_dir)
	$(hide)mkdir $@

$(build_tests_dir): | $(build_dir)
	$(hide)mkdir $@

.PHONY: clean
clean:
	rm -rf $(build_dir)
