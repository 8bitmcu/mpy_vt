FROM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential libffi-dev git pkg-config python3 cmake \
    && rm -rf /var/lib/apt/lists/*

CMD ["/bin/bash"]
