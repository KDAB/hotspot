FROM ubuntu:17.04

RUN apt-get update && apt-get install -y \
    build-essential \
    gettext \
    libkf5threadweaver-dev \
    libkf5i18n-dev \
    libkf5configwidgets-dev \
    libkf5coreaddons-dev \
    libkf5itemviews-dev \
    libkf5itemmodels-dev \
    libelf-dev \
    libdw-dev \
    cmake \
    extra-cmake-modules
