CUDA_VER ?= 12.6

ifeq ($(CUDA_VER),)
  $(error "CUDA_VER is not set")
endif

APP := app

TARGET_DEVICE := $(shell gcc -dumpmachine | cut -f1 -d -)

NVDS_VERSION := 7.1

# Directories

LIB_INSTALL_DIR ?= /opt/nvidia/deepstream/deepstream-$(NVDS_VERSION)/lib/
APP_INSTALL_DIR ?= /opt/nvidia/deepstream/deepstream-$(NVDS_VERSION)/bin/
INCLUDES_DIR ?= /opt/nvidia/deepstream/deepstream-$(NVDS_VERSION)/sources/includes/

SRC_DIR := src
BUILD_DIR := build
INCLUDE_DIR := include
PARSER_DIR := parsers

# Sources

C_SRCS := $(shell find $(SRC_DIR) -name '*.c')
CPP_SRCS := $(shell find $(SRC_DIR) -name '*.cpp')
CUDA_SRCS := $(shell find $(SRC_DIR) -name '*.cu')
INCS := $(shell find $(INCLUDE_DIR) -name '*.h')

# Objects

C_OBJS := $(C_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
CPP_OBJS := $(CPP_SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
CUDA_OBJS := $(CUDA_SRCS:$(SRC_DIR)/%.cu=$(BUILD_DIR)/%.o)
OBJS := $(C_OBJS) $(CPP_OBJS) $(CUDA_OBJS)

# Packages

PKGS := \
    gstreamer-1.0 \
    gstreamer-rtsp-server-1.0 \
    yaml-cpp

# Compilers

CC := gcc
CXX := g++
NVCC := /usr/local/cuda-$(CUDA_VER)/bin/nvcc

# Common Flags

COMMON_FLAGS := \
    -I$(INCLUDES_DIR) \
    -I/usr/local/cuda-$(CUDA_VER)/include \
    -I$(INCLUDE_DIR) \
    -I$(SRC_DIR)

# C Flags

CFLAGS += $(COMMON_FLAGS)
CFLAGS += \
    $(shell pkg-config --cflags $(PKGS))

# C++ Flags

CXXFLAGS += $(COMMON_FLAGS)
CXXFLAGS += \
    -std=c++17 \
    -pthread
CXXFLAGS += \
    $(shell pkg-config --cflags $(PKGS))

# NVCC Flags

RAW_PKG_FLAGS := \
    $(shell pkg-config --cflags $(PKGS))
NVCCFLAGS := \
    $(COMMON_FLAGS)
NVCCFLAGS += \
    -std=c++17
NVCCFLAGS += \
    $(filter-out -pthread,$(RAW_PKG_FLAGS))
NVCCFLAGS += \
    --compiler-options -pthread
NVCCFLAGS += \
    --extended-lambda

# Libraries

LIBS := \
    $(shell pkg-config --libs $(PKGS))
LIBS += \
    -L/usr/local/cuda-$(CUDA_VER)/lib64
LIBS += \
    -lcudart \
    -lcublas \
    -lcuda
LIBS += \
    -lcjson
LIBS += \
    -lm
LIBS += \
    -L$(LIB_INSTALL_DIR)
LIBS += \
    -lnvdsgst_helper \
    -lnvdsgst_meta \
    -lnvds_meta \
    -lnvds_yml_parser \
    -lnvbufsurftransform \
    -lnvbufsurface
LIBS += \
    -Wl,-rpath,$(LIB_INSTALL_DIR)

# Targets

.PHONY: all install clean

all: $(PARSER_LIB) $(APP)

# Custom Parser

PARSER_LIB := \
    $(PARSER_DIR)/libnvdsinfer_custom_yoloface.so

$(PARSER_LIB): \
    $(PARSER_DIR)/yolo_face_parser.cpp

	$(CXX) \
        -shared \
        -fPIC \
        $< \
        -o $@ \
        -I$(INCLUDES_DIR) \
        -I/usr/local/cuda-$(CUDA_VER)/include

# Build Directory

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# C Compilation

$(BUILD_DIR)/%.o: \
    $(SRC_DIR)/%.c Makefile

	@mkdir -p $(dir $@)

	$(CC) \
        -c $< \
        -o $@ \
        $(CFLAGS)

# C++ Compilation

$(BUILD_DIR)/%.o: \
    $(SRC_DIR)/%.cpp Makefile

	@mkdir -p $(dir $@)

	$(CXX) \
        -c $< \
        -o $@ \
        $(CXXFLAGS)

# CUDA Compilation

$(BUILD_DIR)/%.o: \
    $(SRC_DIR)/%.cu Makefile

	@mkdir -p $(dir $@)

	$(NVCC) \
        -c $< \
        -o $@ \
        $(NVCCFLAGS) \
        -x cu

$(APP): $(OBJS)

	$(CXX) \
        -o $(APP) \
        $(OBJS) \
        $(LIBS)

# Install

install: $(APP)

	cp -rv $(APP) $(APP_INSTALL_DIR)

# Clean

clean:

	rm -rf $(BUILD_DIR) $(APP)