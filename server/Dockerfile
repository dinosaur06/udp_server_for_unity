FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN cmake -S . -B build && cmake --build build --target fortress_server

EXPOSE 9000/udp

CMD ["./build/fortress_server"]
