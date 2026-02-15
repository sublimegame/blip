FROM android-build-env AS builder

RUN git clone --depth 1 --branch OpenSSL_1_1_1-stable https://github.com/openssl/openssl.git .

ENV CC=clang
ENV PATH=$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin:$PATH
ENV PATH=$ANDROID_NDK_HOME/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin:$PATH
#RUN which clang

# ARM
RUN ./Configure android-arm -D__ANDROID_API__=$VX_ANDROID_API
RUN make
RUN mkdir -p /output/armeabi-v7a/include \
    && mkdir -p /output/armeabi-v7a/lib \
    && cp -R ./include /output/armeabi-v7a \
    && cp ./libcrypto.a /output/armeabi-v7a/lib/libcrypto.a \
    && cp ./libssl.a /output/armeabi-v7a/lib/libssl.a

RUN make clean

# ARM64
RUN ./Configure android-arm64 -D__ANDROID_API__=$VX_ANDROID_API
RUN make
RUN mkdir -p /output/arm64-v8a/include \
    && mkdir -p /output/arm64-v8a/lib \
    && cp -R ./include /output/arm64-v8a \
    && cp ./libcrypto.a /output/arm64-v8a/lib/libcrypto.a \
    && cp ./libssl.a /output/arm64-v8a/lib/libssl.a

RUN make clean

# x86
RUN ./Configure android-x86 -D__ANDROID_API__=$VX_ANDROID_API
RUN make
RUN mkdir -p /output/x86/include \
    && mkdir -p /output/x86/lib \
    && cp -R ./include /output/x86 \
    && cp ./libcrypto.a /output/x86/lib/libcrypto.a \
    && cp ./libssl.a /output/x86/lib/libssl.a

RUN make clean

# x86_64
RUN ./Configure android-x86_64 -D__ANDROID_API__=$VX_ANDROID_API
RUN make
RUN mkdir -p /output/x86_64/include \
    && mkdir -p /output/x86_64/lib \
    && cp -R ./include /output/x86_64 \
    && cp ./libcrypto.a /output/x86_64/lib/libcrypto.a \
    && cp ./libssl.a /output/x86_64/lib/libssl.a

WORKDIR /output
CMD cp -a . /export/
