FROM voxowl/android-build-env
#FROM android-build-env AS builder

RUN apt-get update && apt-get install -y cmake

WORKDIR /project

# clone libwebsockets repository
RUN git clone --depth 1 --branch v4.3-stable-fix-android https://github.com/voxowl/libwebsockets.git .

# ENV CC=clang
ENV PATH=$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin:$PATH
ENV PATH=$ANDROID_NDK_HOME/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin:$PATH
# path to the NDK version we are using
ENV NDK=$ANDROID_NDK_HOME/$VX_NDK_VERSION

# context is <git-root-dir>/dockerfiles
COPY ./deps/libssl/android /deps/libssl/android

# used to find libssl directories (/deps/libssl/android)
# armv7a
ENV ARM_ANDROID_ABI=armeabi-v7a
# aarch64
ENV ARM64_ANDROID_ABI=arm64-v8a
# i686 
ENV x86_ANDROID_ABI=x86
# x86_64
ENV x86_64_ANDROID_ABI=x86_64

RUN mkdir /project/build
WORKDIR /project/build

# Build for ARM 64bits ("aarch64", "arm64-v8a")
# --------------------------------------------------
RUN rm -f CMakeCache.txt
RUN cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../contrib/cross-aarch64-android.cmake \
    -DLWS_WITHOUT_TESTAPPS=1 \
    -DLWS_OPENSSL_LIBRARIES="/deps/libssl/android/${ARM64_ANDROID_ABI}/lib/libssl.a;/deps/libssl/android/${ARM64_ANDROID_ABI}/lib/libcrypto.a" \
    -DLWS_OPENSSL_INCLUDE_DIRS="/deps/libssl/android/${ARM64_ANDROID_ABI}/include" \
    -DANDROID_API_VER=$VX_ANDROID_API \
    -DNDK=$ANDROID_NDK \
    -DABARCH1=arm64 \
    -DCMAKE_SYSTEM_PROCESSOR=aarch64
RUN make

# copy build output (lib/include) into /output
RUN mkdir -p /output/${ARM64_ANDROID_ABI}/lib
RUN cp /project/build/lib/libwebsockets.a /output/${ARM64_ANDROID_ABI}/lib/libwebsockets.a

RUN mkdir -p /output/${ARM64_ANDROID_ABI}
RUN cp -R /project/build/include /output/${ARM64_ANDROID_ABI}

RUN make clean

# Build for ARM 32bits ("armv7a", "armeabi-v7a")
# --------------------------------------------------
RUN rm -f CMakeCache.txt
RUN cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../contrib/cross-armv7-android.cmake \
    -DLWS_WITHOUT_TESTAPPS=1 \
    -DLWS_OPENSSL_LIBRARIES="/deps/libssl/android/${ARM_ANDROID_ABI}/lib/libssl.a;/deps/libssl/android/${ARM_ANDROID_ABI}/lib/libcrypto.a" \
    -DLWS_OPENSSL_INCLUDE_DIRS="/deps/libssl/android/${ARM_ANDROID_ABI}/include" \
    -DANDROID_API_VER=$VX_ANDROID_API \
    -DNDK=$ANDROID_NDK \
    -DABARCH1=arm \
    -DCMAKE_SYSTEM_PROCESSOR=armv7a
RUN make

# copy build output (lib/include) into /output
RUN mkdir -p /output/${ARM_ANDROID_ABI}/lib
RUN cp /project/build/lib/libwebsockets.a /output/${ARM_ANDROID_ABI}/lib/libwebsockets.a

RUN mkdir -p /output/${ARM_ANDROID_ABI}
RUN cp -R /project/build/include /output/${ARM_ANDROID_ABI}

RUN make clean

# Build for x86 32bits ("i686", "x86")
# --------------------------------------------------
RUN rm -f CMakeCache.txt
RUN cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../contrib/cross-x86-android.cmake \
    -DLWS_WITHOUT_TESTAPPS=1 \
    -DLWS_OPENSSL_LIBRARIES="/deps/libssl/android/${x86_ANDROID_ABI}/lib/libssl.a;/deps/libssl/android/${x86_ANDROID_ABI}/lib/libcrypto.a" \
    -DLWS_OPENSSL_INCLUDE_DIRS="/deps/libssl/android/${x86_ANDROID_ABI}/include" \
    -DANDROID_API_VER=$VX_ANDROID_API \
    -DNDK=$ANDROID_NDK \
    -DABARCH1=x86 \
    -DCMAKE_SYSTEM_PROCESSOR=i686
RUN make

# copy build output (lib/include) into /output
RUN mkdir -p /output/${x86_ANDROID_ABI}/lib
RUN cp /project/build/lib/libwebsockets.a /output/${x86_ANDROID_ABI}/lib/libwebsockets.a

RUN mkdir -p /output/${x86_ANDROID_ABI}
RUN cp -R /project/build/include /output/${x86_ANDROID_ABI}

RUN make clean

# Build for x86 64bits ("x86_64", "x86_64")
# --------------------------------------------------
RUN rm -f CMakeCache.txt
RUN cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=../contrib/cross-x86-android.cmake \
    -DLWS_WITHOUT_TESTAPPS=1 \
    -DLWS_OPENSSL_LIBRARIES="/deps/libssl/android/${x86_64_ANDROID_ABI}/lib/libssl.a;/deps/libssl/android/${x86_64_ANDROID_ABI}/lib/libcrypto.a" \
    -DLWS_OPENSSL_INCLUDE_DIRS="/deps/libssl/android/${x86_64_ANDROID_ABI}/include" \
    -DANDROID_API_VER=$VX_ANDROID_API \
    -DNDK=$ANDROID_NDK \
    -DABARCH1=x86_64 \
    -DCMAKE_SYSTEM_PROCESSOR=x86_64
RUN make

# copy build output (lib/include) into /output
RUN mkdir -p /output/${x86_64_ANDROID_ABI}/lib
RUN cp /project/build/lib/libwebsockets.a /output/${x86_64_ANDROID_ABI}/lib/libwebsockets.a

RUN mkdir -p /output/${x86_64_ANDROID_ABI}
RUN cp -R /project/build/include /output/${x86_64_ANDROID_ABI}

RUN make clean

WORKDIR /output
CMD cp -a . /export/
