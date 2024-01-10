FROM debian:latest
RUN apt-get update && apt-get install -y clang llvm g++ gcc
