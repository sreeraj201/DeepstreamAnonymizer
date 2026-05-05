CUDA_VER?=12.6
ifeq ($(CUDA_VER),)
  $(error "CUDA_VER is not set")
endif

APP:= app

TARGET_DEVICE = $(shell gcc -dumpmachine | cut -f1 -d -)

NVDS_VERSION:=7.1

LIB_INSTALL_DIR?=/opt/nvidia/deepstream/deepstream-$(NVDS_VERSION)/lib/
APP_INSTALL_DIR?=/opt/nvidia/deepstream/deepstream-$(NVDS_VERSION)/bin/
INCLUDES_DIR?=/opt/nvidia/deepstream/deepstream-$(NVDS_VERSION)/sources/includes/


# Sources
C_SRCS   := $(wildcard *.c)
CPP_SRCS := $(wildcard *.cpp)
INCS     := $(wildcard *.h)

PKGS:= gstreamer-1.0

# Objects
C_OBJS   := $(C_SRCS:.c=.o)
CPP_OBJS := $(CPP_SRCS:.cpp=.o)
OBJS     := $(C_OBJS) $(CPP_OBJS)

# Compilers
CC  := gcc
CXX := g++

# Flags
CFLAGS += -I$(INCLUDES_DIR) \
          -I /usr/local/cuda-$(CUDA_VER)/include
CFLAGS += $(shell pkg-config --cflags $(PKGS))

CXXFLAGS += -I$(INCLUDES_DIR) \
            -I /usr/local/cuda-$(CUDA_VER)/include \
            -std=c++17
CXXFLAGS += $(shell pkg-config --cflags $(PKGS))

# Libraries
LIBS:= $(shell pkg-config --libs $(PKGS))

LIBS+= -L/usr/local/cuda-$(CUDA_VER)/lib64/ -lcudart -lnvdsgst_helper -lm \
       -L$(LIB_INSTALL_DIR) -lnvdsgst_meta -lnvds_meta -lnvds_yml_parser \
       -lnvbufsurftransform -lnvbufsurface \
       -lcuda -Wl,-rpath,$(LIB_INSTALL_DIR)

all: $(APP)

# C compilation
%.o: %.c $(INCS) Makefile
	$(CC) -c -o $@ $(CFLAGS) $<

# C++ compilation
%.o: %.cpp $(INCS) Makefile
	$(CXX) -c -o $@ $(CXXFLAGS) $<

# IMPORTANT: link with CXX (not CC)
$(APP): $(OBJS) Makefile
	$(CXX) -o $(APP) $(OBJS) $(LIBS)

install: $(APP)
	cp -rv $(APP) $(APP_INSTALL_DIR)

clean:
	rm -rf $(OBJS) $(APP)
