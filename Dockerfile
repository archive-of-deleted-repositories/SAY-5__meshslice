FROM ubuntu:24.04 AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git ca-certificates \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMESHSLICE_BUILD_TESTS=OFF -DMESHSLICE_BUILD_BENCH=OFF \
    && cmake --build build -j "$(nproc)" --target meshslice

FROM ubuntu:24.04 AS runtime
RUN useradd -m -u 10001 meshslice
COPY --from=build /src/build/meshslice /usr/local/bin/meshslice
USER meshslice
ENTRYPOINT ["meshslice"]
CMD ["--help"]
