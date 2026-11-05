# ---------- Build stage ----------
FROM debian:stable-slim AS build

RUN apt-get update && \
    apt-get install -y --no-install-recommends gcc libc6-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY server.c .

# Static binary: it needs no libc at runtime, so it can run from an empty image
RUN gcc -O2 -Wall -Wextra -static -o server server.c && strip server


# ---------- Runtime stage ----------
FROM scratch

COPY --from=build /src/server /server

# Run as an unprivileged user (scratch has no /etc/passwd, so use numeric IDs)
USER 65534:65534

EXPOSE 8080

# The server handles SIGTERM itself, so `docker stop` returns immediately
ENTRYPOINT ["/server"]
