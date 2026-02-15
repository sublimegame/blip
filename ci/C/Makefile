
# --------------------------------------------------
# Convert architecture name from Docker to CMake format
# --------------------------------------------------
PLATFORM_ARCH_CMAKE=linux-x86_64 # default
ifeq ($(CUBZH_TARGETARCH),arm64)
	PLATFORM_ARCH_CMAKE=linux-aarch64
else ifeq ($(CUBZH_TARGETARCH),amd64)
	PLATFORM_ARCH_CMAKE=linux-x86_64
endif
echo "PLATFORM/ARCH: "$(PLATFORM_ARCH_CMAKE)

VX_APP_BUILD_TARGET_LINUX_LUAUNITTESTS = linux_lua_unit_tests

# --------------------------------------------------
COMMON_DIR=/common
LIBLUAU_DIR=/deps/libluau

LIBPNG_DIR=/deps/lpng/src
LIBMINIAUDIO_DIR=/deps/miniaudio
LIBWEBSOCKETS_DIR=/deps/libwebsockets/linux/$(CUBZH_TARGETARCH)/include
LIBSSL_DIR=/deps/libssl/linux/$(CUBZH_TARGETARCH)/include
LIBVXTOOLS_DIR=/deps/xptools
VXLUASANDBOX_DIR=$(COMMON_DIR)/VXLuaSandbox
VXFRAMEWORK_DIR=$(COMMON_DIR)/VXFramework
VXGAMESERVER_DIR=$(COMMON_DIR)/VXGameServer
VXNETWORKING_DIR=$(COMMON_DIR)/VXNetworking

# Cubzh Core
CUBZH_CORE_DIR=/core

# Cubzh Core Dependencies
LIBZ_INCLUDE_DIR=/deps/libz/$(PLATFORM_ARCH_CMAKE)/include
LIBZ_LIBS_DIR=/deps/libz/$(PLATFORM_ARCH_CMAKE)/lib

all: deps ci

#
# TODO: gdevillele: once the go lib is removed, we should be able to remove "-pthread"
#
ci: clean-ci

	# 1 (C)

	@gcc \
	-D GRPC_OFF \
	-D DEBUG \
	-D CLIENT_ENV \
	-D P3S_CLIENT_HEADLESS \
	-D __VX_USE_LIBWEBSOCKETS \
	-D __VX_PLATFORM_LINUX \
	-D __VX_APP_BUILD_TARGET=$(VX_APP_BUILD_TARGET_LINUX_LUAUNITTESTS) \
	-pthread \
	-c $(CUBZH_CORE_DIR)/*.c \
	-I$(CUBZH_CORE_DIR) \
	-I$(LIBVXTOOLS_DIR)/include \
	-I$(VXNETWORKING_DIR) \
	-I$(LIBLUAU_DIR)/_active_/prebuilt/linux/x86_64/include \
	-I$(LIBPNG_DIR) \
	-I$(LIBMINIAUDIO_DIR) \
	-I$(LIBWEBSOCKETS_DIR) \
	-I$(LIBSSL_DIR) \
	-I$(LIBZ_INCLUDE_DIR)

	# # 2 (C++)

	@gcc \
	-std=c++17 \
	-D GRPC_OFF \
	-D DEBUG \
	-D CLIENT_ENV \
	-D P3S_CLIENT_HEADLESS \
	-D __VX_USE_LIBWEBSOCKETS \
	-D __VX_PLATFORM_LINUX \
	-D __VX_APP_BUILD_TARGET=$(VX_APP_BUILD_TARGET_LINUX_LUAUNITTESTS) \
	-pthread \
	-c *.cpp \
	-c $(VXFRAMEWORK_DIR)/*.cpp \
	-c $(VXFRAMEWORK_DIR)/*.c \
	-c $(VXLUASANDBOX_DIR)/*.cpp \
	-c $(VXNETWORKING_DIR)/*.cpp -I$(VXNETWORKING_DIR) \
	-c $(VXGAMESERVER_DIR)/VXGameServer.cpp \
	-c $(VXGAMESERVER_DIR)/VXConnectionHandlerJoin.cpp \
	-c $(VXGAMESERVER_DIR)/VXMessageQueues.cpp \
	-c $(VXGAMESERVER_DIR)/VXConnectionHandler.cpp \
	-c $(LIBVXTOOLS_DIR)/deps/*.cpp -I$(LIBVXTOOLS_DIR)/deps \
	-I$(VXGAMESERVER_DIR) \
	-I./ \
	-I$(CUBZH_CORE_DIR) \
	-I$(LIBVXTOOLS_DIR)/include \
	-I$(VXGAMESERVER_DIR) \
	-I$(VXFRAMEWORK_DIR) \
	-I$(VXLUASANDBOX_DIR) \
	-I$(LIBLUAU_DIR)/_active_/prebuilt/linux/x86_64/include \
	-I$(LIBPNG_DIR) \
	-I$(LIBMINIAUDIO_DIR) \
	-I$(LIBWEBSOCKETS_DIR) \
	-I$(LIBSSL_DIR)

	# 3 (linking)

	@gcc \
	-pthread \
	*.o \
	-Wl,--start-group \
	-L$(LIBVXTOOLS_DIR)/linux \
	-ldl \
	-lstdc++ \
	-lm \
	-lxptools \
	-L$(LIBZ_LIBS_DIR) \
	-lz \
	-L$(LIBPNG_DIR) \
	-lpng \
	-L/deps/libssl/linux/$(CUBZH_TARGETARCH)/libs \
	-lcrypto \
	-lssl \
	-L/deps/libwebsockets/linux/$(CUBZH_TARGETARCH)/libs \
	-lwebsockets \
	-L$(LIBLUAU_DIR)/_active_/prebuilt/linux/x86_64/lib-Debug \
	-lluau \
	-Wl,--end-group \
	-o ci \

deps:
	@make --directory "$(LIBVXTOOLS_DIR)/linux" all __VX_APP_BUILD_TARGET=$(VX_APP_BUILD_TARGET_LINUX_LUAUNITTESTS)
	@cd $(LIBPNG_DIR); make --makefile=./scripts/makefile.linux clean; make --makefile=./scripts/makefile.linux ZLIBINC=$(LIBZ_INCLUDE_DIR) ZLIBLIB=$(LIBZ_LIBS_DIR);

clean-ci:
	@rm -f ci

clean:
	@rm -f ci
