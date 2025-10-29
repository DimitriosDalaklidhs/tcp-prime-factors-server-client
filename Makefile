PHONY: lib

TLPI_DIR = ./lib/
TLPI_LIB = ./lib/libtlpi.a
TLPI_INCL_DIR = ${TLPI_DIR}/lib

include ./lib/Makefile.inc

GEN_EXE = server

LINUX_EXE = large_file

EXE = ${GEN_EXE} ${LINUX_EXE}

all : lib exe

exe : ${EXE}

allgen : ${GEN_EXE}
