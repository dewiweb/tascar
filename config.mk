# variables:
VERSION=0.211.0
CXXFLAGS = -Wall -Wno-deprecated-declarations -msse -msse2		\
-mfpmath=sse -ffast-math -fno-finite-math-only -fext-numeric-literals	\
-std=c++11 -pthread -ggdb
CPPFLAGS = -std=c++11
PREFIX = /usr/local

GITMODIFIED=$(shell test -z "`git status --porcelain -uno`" || echo "-modified")
COMMITHASH=$(shell git log -1 --abbrev=7 --pretty='format:%h')
COMMIT_SINCE_MASTER=$(shell git log --pretty='format:%h' origin/master.. | wc -w)

FULLVERSION=$(VERSION).$(COMMIT_SINCE_MASTER)-$(COMMITHASH)$(GITMODIFIED)

mkfile_name := $(abspath $(lastword $(MAKEFILE_LIST)))
mkfile_path := $(subst $(notdir $(mkfile_name)),,$(mkfile_name))

HAS_LSL=$(shell $(mkfile_path)/check_for_lsl)

HAS_OPENMHA=$(shell $(mkfile_path)/check_for_openmha)

HAS_OPENCV2=$(shell $(mkfile_path)/check_for_opencv2)

HAS_OPENCV4=$(shell $(mkfile_path)/check_for_opencv4)

BUILD_DIR = build
SOURCE_DIR = src

export VERSION
export SOURCE_DIR
export BUILD_DIR
export CXXFLAGS
export HAS_LSL
export HAS_OPENMHA
export HAS_OPENCV2
