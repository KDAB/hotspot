# trusty
FROM ubuntu:14.04 as package_d_demangle_intermediate

# install dependencies
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get -y upgrade && \
    apt-get install -y software-properties-common build-essential curl unzip

RUN curl -fsS https://dlang.org/install.sh | bash -s dmd
FROM package_d_demangle_intermediate

WORKDIR /opt
ADD . /opt/

RUN curl -L https://github.com/lievenhey/d_demangler/archive/refs/tags/version-0.0.2.zip -o d_demangle.zip && unzip d_demangle.zip
CMD ./build_d_demangle.sh
