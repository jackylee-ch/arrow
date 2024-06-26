#!/usr/bin/env bash
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

set -e

if [  "$#" -lt 1 -o "$#" -gt 3 ]; then
    echo "Usage: $0 <build> <prefix> <arch> <version>"
    echo "Will default to arch=x86_64 and version=0.3.0 "
    exit 1
fi

BUILD=$1
PREFIX=$2
ARCH=${3:-x86_64}
VERSION=${4:-0.3.0}

SCCACHE_URL="https://github.com/mozilla/sccache/releases/download/v$VERSION/sccache-v$VERSION-$ARCH-$BUILD.tar.gz"
SCCACHE_ARCHIVE=sccache.tar.gz

# Download archive and checksum
curl -L $SCCACHE_URL --output $SCCACHE_ARCHIVE
curl -L $SCCACHE_URL.sha256 --output $SCCACHE_ARCHIVE.sha256

echo "$(cat $SCCACHE_ARCHIVE.sha256) $SCCACHE_ARCHIVE" | sha256sum --check --status

if [ ! -d $PREFIX ]; then
    mkdir -p $PREFIX
fi

tar -xzvf $SCCACHE_ARCHIVE --strip-component=1 --directory $PREFIX --wildcards sccache*/sccache* 
chmod u+x $PREFIX/sccache

if [ "${GITHUB_ACTIONS}" = "true" ]; then
    echo "$PREFIX" >> $GITHUB_PATH
    # Add executable for windows as mingw workaround.
    echo "SCCACHE_PATH=$PREFIX/sccache.exe" >> $GITHUB_ENV
fi
