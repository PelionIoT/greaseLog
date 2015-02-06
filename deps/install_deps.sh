#!/bin/bash

# http://stackoverflow.com/questions/59895/can-a-bash-script-tell-what-directory-its-stored-in
SOURCE="${BASH_SOURCE[0]}"
while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
  SELF="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
  SOURCE="$(readlink "$SOURCE")"
  [[ $SOURCE != /* ]] && SOURCE="$DIR/$SOURCE" # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done

DEPS_DIR="$( cd -P "$( dirname "$SOURCE" )" && pwd )"
LOG=${DEPS_DIR}/../install_deps.log
GPERF_DIR=${DEPS_DIR}/gperftools-2.4

pushd $GPERF_DIR

touch $LOG
cp configure.orig configure
(./configure $CONFIG_OPTIONS --prefix=${DEPS_DIR}/build --enable-frame-pointers --with-pic 2>&1 >> $LOG && make -j4 2>&1 >> $LOG && make install 2>&1 >> $LOG) || echo "Error building gperftools-2.4"
popd
