FROM gcc:12 AS builder

WORKDIR /app
COPY . .
RUN make clean && make

FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends \
    libc6-dev \
    libpthread-stubs0-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/shell .

EXPOSE 8080
CMD ["./shell", "--web"]
