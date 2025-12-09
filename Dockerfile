# ---------- Build stage ----------
FROM debian:stable-slim AS build

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src

# copy source file
COPY server.c .

# compile the server
RUN gcc -O2 -Wall -o server server.c && strip server


# ---------- Runtime stage ----------
FROM debian:stable-slim

WORKDIR /app

# copy the built binary from the build stage
COPY --from=build /src/server .

EXPOSE 8080

# run the server when the container starts
CMD ["./server"]
