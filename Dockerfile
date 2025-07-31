FROM ubuntu:22.04 AS builder

# Install tools first
RUN apt-get update && apt-get install -y \
    build-essential \
    make \
    dpkg \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy all files including MTA.deb and sources
COPY . .

# Install MTA.deb to get mta_crypt.h and any required libraries
RUN dpkg -i MTA.deb

# Now build using your Makefile.enc (mta_crypt.h should now be available)
RUN make -f Makefile.enc

# Runtime image
FROM ubuntu:22.04
WORKDIR /app

# Copy built binary from builder stage
COPY --from=builder /app/new_encryptor .

# Default command
CMD ["./new_encryptor"]

