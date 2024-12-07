# Use an official GCC image
FROM gcc:12

# Accept build argument for profiling
ARG ENABLE_PROFILING=false

# Set profiling environment variable based on build argument
ENV ENABLE_PROFILING=${ENABLE_PROFILING}

# Install dependencies
RUN apt-get update && \
    apt-get install -y \
    cmake \
    git \
    build-essential \
    autoconf \
    libtool \
    pkg-config \
    wget \
    valgrind \
    libprotobuf-dev \
    protobuf-compiler \
    libmysqlcppconn-dev \
    libssl-dev \
    google-perftools \
    libgoogle-perftools-dev \
    binutils \
    linux-perf && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# Clone and build gRPC and Protobuf
WORKDIR /tmp
RUN git clone --recurse-submodules --depth 1 --shallow-submodules https://github.com/grpc/grpc.git && \
    cd grpc && mkdir -p cmake/build && cd cmake/build && \
    cmake -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF \
          -DABSL_PROPAGATE_CXX_STD=ON ../.. && \
    make -j$(nproc) && make install && \
    ldconfig

RUN git clone https://github.com/gperftools/gperftools.git && \
    cd gperftools && \
    ./autogen.sh && \
    ./configure --prefix=/usr/local && \
    make -j$(nproc) && make install && \
    ldconfig


# Set working directory
WORKDIR /usr/src/app

# Copy the project files
COPY proto ./proto
COPY server ./server
COPY client ./client
COPY CMakeLists.txt ./CMakeLists.txt

# Build the project
RUN mkdir -p build && cd build && \
    cmake .. && \
    make -j$(nproc)

# Default command
CMD ["./build/server/server"]
