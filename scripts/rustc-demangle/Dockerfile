# trusty
FROM ubuntu:14.04 as package_rustc_demangle_intermediate

# install dependencies
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get -y upgrade && \
    apt-get install -y software-properties-common build-essential curl unzip

RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs -o rustup.sh && chmod +x rustup.sh && ./rustup.sh -y

FROM package_rustc_demangle_intermediate

WORKDIR /opt
ADD . /opt/

RUN curl -L https://github.com/alexcrichton/rustc-demangle/archive/master.zip -o rustc_demangle.zip && unzip rustc_demangle.zip
ENV PATH="/root/.cargo/bin:${PATH}"
CMD ./build_rustc_demangle.sh /opt /artifacts /opt/rustc-demangle-master
