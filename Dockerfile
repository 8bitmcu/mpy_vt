FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential libffi-dev git pkg-config python3 cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt
RUN git clone --depth 1 https://github.com/micropython/micropython.git
RUN make -C micropython/mpy-cross
WORKDIR /opt/micropython/ports/unix
RUN make submodules

RUN mkdir -p build-standard

CMD ["/bin/bash"]
