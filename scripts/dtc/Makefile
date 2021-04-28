# SPDX-License-Identifier: GPL-2.0
# scripts/dtc makefile

hostprogs-y	:= dtc
always		:= $(hostprogs-y)

dtc-objs	:= dtc.o flattree.o fstree.o data.o livetree.o treesource.o \
		   srcpos.o checks.o util.o
dtc-objs	+= dtc-lexer.lex.o dtc-parser.tab.o

dtc-objs	+= libfdt/fdt.o libfdt/fdt_ro.o libfdt/fdt_wip.o libfdt/fdt_sw.o \
			libfdt/fdt_rw.o libfdt/fdt_strerror.o libfdt/fdt_empty_tree.o \
			libfdt/fdt_addresses.o libfdt/fdt_overlay.o libfdt/dtbo_merge_check.o
# Source files need to get at the userspace version of libfdt_env.h to compile
HOST_EXTRACFLAGS := -I$(src)/libfdt

ifeq ($(wildcard /usr/include/yaml.h),)
HOST_EXTRACFLAGS += -DNO_YAML
else
dtc-objs	+= yamltree.o
HOSTLDLIBS_dtc	:= -lyaml
endif

# Generated files need one more search path to include headers in source tree
HOSTCFLAGS_dtc-lexer.lex.o := -I$(src)
HOSTCFLAGS_dtc-parser.tab.o := -I$(src)

# dependencies on generated files need to be listed explicitly
$(obj)/dtc-lexer.lex.o: $(obj)/dtc-parser.tab.h

# generated files need to be cleaned explicitly
clean-files	:= dtc-lexer.lex.c dtc-parser.tab.c dtc-parser.tab.h
