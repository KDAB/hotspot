# trusty
FROM ubuntu:14.04 as package_d_demangle_intermediate

# install dependencies
ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get -y upgrade && \
    apt-get install -y software-properties-common build-essential curl

RUN curl -fsS https://dlang.org/install.sh | bash -s dmd

FROM package_d_demangle_intermediate

WORKDIR /opt
ADD ddemangle.d /opt/

RUN $(/root/dlang/install.sh get-path) ddemangle.d -O -shared -fPIC -defaultlib=libphobos2.a -of=libddemangle.so && \
    mkdir -p package/opt/lib && \
    cp libddemangle.so package/opt/lib && \
    tar cjvf ddemangle.build.tar.bz -C ./package opt
