#!/usr/bin/env bash

set -euo pipefail

sudo apt-get update
sudo apt-get install -y \
    curl \
    g++ \
    libboost-all-dev \
    libmysqlclient-dev \
    libssl-dev \
    make \
    mysql-server

curl --fail --silent --show-error --location \
    https://github.com/CrowCpp/Crow/releases/download/v1.3.2/crow_all.h \
    --output include/crow_all.h
