# derived from https://github.com/mingchen/docker-android-build-box/blob/master/Dockerfile
# removing things such as nodejs

FROM ubuntu:20.04 AS builder

# ARG VX_NDK_VERSION/VX_ANDROID_API
ENV VX_NDK_VERSION="21.4.7075529"
ENV VX_ANDROID_API="24"

ENV ANDROID_HOME="/opt/android-sdk" 
ENV ANDROID_SDK_HOME=$ANDROID_HOME

ENV ANDROID_NDK="/opt/android-sdk/ndk/current"
ENV ANDROID_NDK_HOME=$ANDROID_NDK

# support amd64 and arm64
RUN JDK_PLATFORM=$(if [ "$(uname -m)" = "aarch64" ]; then echo "arm64"; else echo "amd64"; fi) && \
    echo export JDK_PLATFORM=$JDK_PLATFORM >> /etc/jdk.env && \
    echo export JAVA_HOME="/usr/lib/jvm/java-11-openjdk-$JDK_PLATFORM/" >> /etc/jdk.env && \
    echo . /etc/jdk.env >> /etc/bash.bashrc && \
    echo . /etc/jdk.env >> /etc/profile

ENV TZ=America/Los_Angeles

# Get the latest version from https://developer.android.com/studio/index.html
ENV ANDROID_SDK_TOOLS_VERSION="commandlinetools-linux-8092744_latest"

# Set locale
ENV LANG="en_US.UTF-8" \
    LANGUAGE="en_US.UTF-8" \
    LC_ALL="en_US.UTF-8"

RUN apt-get clean && \
    apt-get update -qq && \
    apt-get install -qq -y apt-utils locales && \
    locale-gen $LANG

ENV DEBIAN_FRONTEND="noninteractive" \
    TERM=dumb \
    DEBIAN_FRONTEND=noninteractive

ENV PATH="$JAVA_HOME/bin:$PATH:$ANDROID_SDK_HOME/emulator:$ANDROID_SDK_HOME/cmdline-tools/bin:$ANDROID_SDK_HOME/tools:$ANDROID_SDK_HOME/platform-tools:$ANDROID_NDK"

WORKDIR /tmp

# Installing packages
RUN apt-get update -qq > /dev/null && \
    apt-get install -qq locales > /dev/null && \
    locale-gen "$LANG" > /dev/null && \
    apt-get install -qq --no-install-recommends \
        autoconf \
        build-essential \
        curl \
        file \
        git \
        gpg-agent \
        less \
        libc6-dev \
        libgmp-dev \
        libmpc-dev \
        libmpfr-dev \
        libxslt-dev \
        libxml2-dev \
        m4 \
        ncurses-dev \
        ocaml \
        openjdk-11-jdk \
        openssh-client \
        pkg-config \
        software-properties-common \
        tzdata \
        unzip \
        vim-tiny \
        wget \
        zip \
        zlib1g-dev > /dev/null && \
    echo "JVM directories: `ls -l /usr/lib/jvm/`" && \
    . /etc/jdk.env && \
    echo "Java version (default):" && \
    java -version && \
    echo "set timezone" && \
    ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone && \
    rm -rf /var/lib/apt/lists/ && \
    rm -rf /tmp/* /var/tmp/*

# Install Android command line tools
RUN echo "sdk tools ${ANDROID_SDK_TOOLS_VERSION}" && \
    wget --quiet --output-document=sdk-tools.zip \
        "https://dl.google.com/android/repository/${ANDROID_SDK_TOOLS_VERSION}.zip" && \
    mkdir --parents "$ANDROID_HOME" && \
    unzip -q sdk-tools.zip -d "$ANDROID_HOME" && \
    rm --force sdk-tools.zip


# List all available packages.
# redirect to a temp file `packages.txt` for later use and avoid show progress
RUN . /etc/jdk.env && \
    ${ANDROID_HOME}/cmdline-tools/bin/sdkmanager --list --sdk_root=${ANDROID_HOME} > packages.txt && \
    cat packages.txt | grep -v '='

#
# https://developer.android.com/studio/command-line/sdkmanager.html
#
RUN echo "platforms" && \
    . /etc/jdk.env && \
    yes | "$ANDROID_HOME"/cmdline-tools/bin/sdkmanager --sdk_root=${ANDROID_HOME} \
        "platforms;android-$VX_ANDROID_API" > /dev/null

RUN echo "platform tools" && \
    . /etc/jdk.env && \
    yes | "$ANDROID_HOME"/cmdline-tools/bin/sdkmanager --sdk_root=${ANDROID_HOME} \
        "platform-tools" > /dev/null

RUN echo "build tools 31" && \
    . /etc/jdk.env && \
    yes | "$ANDROID_HOME"/cmdline-tools/bin/sdkmanager --sdk_root=${ANDROID_HOME} \
        "build-tools;31.0.0" > /dev/null

# seems there is no emulator on arm64
# Warning: Failed to find package emulator
# RUN echo "emulator" && \
#     if [ "$(uname -m)" != "x86_64" ]; then echo "emulator only support Linux x86 64bit. skip for $(uname -m)"; exit 0; fi && \
#     . /etc/jdk.env && \
#     yes | "$ANDROID_HOME"/cmdline-tools/bin/sdkmanager "emulator" > /dev/null

# ndk-bundle does exist on arm64
RUN echo "NDK" && \
    yes | "$ANDROID_HOME"/cmdline-tools/bin/sdkmanager --sdk_root=${ANDROID_HOME} "ndk-bundle" > /dev/null

RUN echo "Installing NDK $VX_NDK_VERSION" && \
    . /etc/jdk.env && \
    yes | "$ANDROID_HOME"/cmdline-tools/bin/sdkmanager --sdk_root=${ANDROID_HOME} "ndk;$VX_NDK_VERSION" > /dev/null && \
    ln -sv $ANDROID_HOME/ndk/${VX_NDK_VERSION} ${ANDROID_NDK}

# RUN echo "NDK" && \
#     NDK=$(grep 'ndk;' packages.txt | sort | tail -n1 | awk '{print $1}') && \
#     NDK_VERSION=$(echo $NDK | awk -F\; '{print $2}') && \
#     echo "Installing $NDK $NDK_VERSION" && \
#     . /etc/jdk.env && \
#     yes | "$ANDROID_HOME"/cmdline-tools/bin/sdkmanager --sdk_root=${ANDROID_HOME} "$NDK" > /dev/null && \
#     ln -sv $ANDROID_HOME/ndk/${NDK_VERSION} ${ANDROID_NDK}

# List sdk and ndk directory content
RUN ls -l $ANDROID_HOME && \
    ls -l $ANDROID_HOME/ndk && \
    ls -l $ANDROID_HOME/ndk/*

RUN du -sh $ANDROID_HOME

# Copy sdk license agreement files.
# RUN mkdir -p $ANDROID_HOME/licenses
# COPY sdk/licenses/* $ANDROID_HOME/licenses/

# # Create some jenkins required directory to allow this image run with Jenkins
# # RUN mkdir -p /var/lib/jenkins/workspace && \
# #    mkdir -p /home/jenkins && \
# #    chmod 777 /home/jenkins && \
# #    chmod 777 /var/lib/jenkins/workspace && \
# #    chmod -R 775 $ANDROID_HOME

# # Add jenv to control which version of java to use, default to 11.
# RUN git clone https://github.com/jenv/jenv.git ~/.jenv && \
#     echo 'export PATH="$HOME/.jenv/bin:$PATH"' >> ~/.bash_profile && \
#     echo 'eval "$(jenv init -)"' >> ~/.bash_profile && \
#     . ~/.bash_profile && \
#     . /etc/jdk.env && \
#     java -version && \
#     jenv add /usr/lib/jvm/java-8-openjdk-$JDK_PLATFORM && \
#     jenv add /usr/lib/jvm/java-11-openjdk-$JDK_PLATFORM && \
#     jenv versions && \
#     jenv global 11 && \
#     java -version

WORKDIR /project
