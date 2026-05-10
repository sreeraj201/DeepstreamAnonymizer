CUDA_VER ?= 12.6

ifeq ($(CUDA_VER),)
  $(error "CUDA_VER is not set")
endif

APP := app

TARGET_DEVICE := $(shell gcc -dumpmachine | cut -f1 -d -)
NVDS_VERSION := 7.1

LIB_INSTALL_DIR ?= /opt/nvidia/deepstream/deepstream-$(NVDS_VERSION)/lib/
APP_INSTALL_DIR ?= /opt/nvidia/deepstream/deepstream-$(NVDS_VERSION)/bin/
INCLUDES_DIR ?= /opt/nvidia/deepstream/deepstream-$(NVDS_VERSION)/sources/includes/

# Directories
SRC_DIR := src
BUILD_DIR := build

# Sources
C_SRCS   := $(wildcard $(SRC_DIR)/*.c)
CPP_SRCS := $(wildcard $(SRC_DIR)/*.cpp)
INCS     := $(wildcard $(SRC_DIR)/*.h)

# Objects
C_OBJS   := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SRCS))
CPP_OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(CPP_SRCS))
OBJS     := $(C_OBJS) $(CPP_OBJS)

# Packages
PKGS := gstreamer-1.0 \
        gstreamer-rtsp-server-1.0 \
        yaml-cpp

# Compilers
CC  := gcc
CXX := g++

# Compiler flags
COMMON_FLAGS := \
    -I$(INCLUDES_DIR) \
    -I/usr/local/cuda-$(CUDA_VER)/include \
    -I$(SRC_DIR)

CFLAGS += $(COMMON_FLAGS)
CFLAGS += $(shell pkg-config --cflags $(PKGS))

CXXFLAGS += $(COMMON_FLAGS)
CXXFLAGS += -std=c++17
CXXFLAGS += $(shell pkg-config --cflags $(PKGS))

# Libraries
LIBS := $(shell pkg-config --libs $(PKGS))

LIBS += \
    -L/usr/local/cuda-$(CUDA_VER)/lib64/ \
    -lcudart \
    -lcuda \
    -lm \
    -lnvdsgst_helper \
    -L$(LIB_INSTALL_DIR) \
    -lnvdsgst_meta \
    -lnvds_meta \
    -lnvds_yml_parser \
    -lnvbufsurftransform \
    -lnvbufsurface \
    -Wl,-rpath,$(LIB_INSTALL_DIR)

.PHONY: all install clean

all: $(APP)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(INCS) Makefile | $(BUILD_DIR)
	$(CC) -c $< -o $@ $(CFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(INCS) Makefile | $(BUILD_DIR)
	$(CXX) -c $< -o $@ $(CXXFLAGS)

$(APP): $(OBJS)
	$(CXX) -o $(APP) $(OBJS) $(LIBS)

install: $(APP)
	cp -rv $(APP) $(APP_INSTALL_DIR)

clean:
	rm -rf $(BUILD_DIR) $(APP)