# Use an official GCC image with a specific version for consistency
FROM gcc:12

# Install MySQL client libraries and dependencies
RUN apt-get update && \
    apt-get install -y libmysqlcppconn-dev build-essential && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Set the working directory in the container
WORKDIR /usr/src/app

# Copy the C++ source file into the container
COPY hello.cpp .

# Compile the C++ program with MySQL connector
RUN g++ -o hello hello.cpp -lmysqlcppconn

# Set the default command to execute the program
CMD ["./hello"]
