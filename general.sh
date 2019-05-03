#!/bin/sh

if [ "$#" -lt 1 ]; then
 echo "Usage: $0 <--configure|make-command>"
 exit 1
fi

BASEDIR=$(readlink -f "$BASH_SOURCE")
BASEDIR=$(dirname "${BASEDIR}")

if [ -z "$CC" ]; then
  echo "CC not set"
fi
if [ -z "$CXX" ]; then
  echo "CXX not set"
fi
if [ -z "$FC" ]; then
  echo "FC not set"
fi
if [ -z "$MPICC" ]; then
  echo "MPICC not set"
fi
if [ -z "$MPICXX" ]; then
  echo "MPICXX not set"
fi

# Note: This is NOT CMake environment variable
CMAKE_MODULE_PATH="${CMAKE_MODULE_PATH:+:}/home/adz8/codes-work-dir/build-scripts/cmake"

# Note: This is CMake environment variable that enables search for dependencies in specific locations
#CMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH:+:}${INTEL17_TOOLS_PREFIX}/perfoscope/0.1.0"
#CMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}:${INTEL17_TOOLS_PREFIX}/papi/5.5.1"
#CMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}:${INTEL17_TOOLS_PREFIX}/sqlite/3.21.0"
#export CMAKE_PREFIX_PATH

# For debugging only
#echo "**********"
#echo "CMAKE_MODULE_PATH=$CMAKE_MODULE_PATH"
#echo "CMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH"
#echo "**********"

case "$1" in
  -c|--configure)
    if [ "$#" -ne 2 ]; then
      echo "Usage: $0 --configure <build-type>"
      exit 1
    fi

    BUILD_TYPE="$2"

    cmake "$BASEDIR" \
      -DCMAKE_C_COMPILER:STRING="$CC" \
      -DCMAKE_CXX_COMPILER:STRING="$CXX" \
      -DCMAKE_Fortran_COMPILER:STRING="$FC" \
      -DMPIC_COMPILER:STRING="$MPICC" \
      -DMPICXX_COMPILER:STRING="$MPICXX" \
      -DCMAKE_BUILD_TYPE:STRING="$BUILD_TYPE" \
      -DCMAKE_C_FLAGS_DEBUG:STRING="-g -debug inline-debug-info -p -O0 -qno-offload" \
      -DCMAKE_C_FLAGS_RELEASE:STRING="-O3 -DNDEBUG -ip -qno-offload" \
      -DCMAKE_C_FLAGS_RELWITHDEBINFO="-g -debug inline-debug-info -p -O3 -ip -qno-offload" \
      -DCMAKE_CXX_FLAGS_DEBUG:STRING="-g -debug inline-debug-info -p -O0 -qno-offload" \
      -DCMAKE_CXX_FLAGS_RELEASE:STRING="-O3 -DNDEBUG -ip -qno-offload" \
      -DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-g -debug inline-debug-info -p -O3 -ip -qno-offload" \
      -DCMAKE_Fortran_FLAGS_DEBUG:STRING="-g -debug inline-debug-info  -p -O0 -qno-offload" \
      -DCMAKE_Fortran_FLAGS_RELEASE:STRING="-O3 -DNDEBUG -ip -qno-offload" \
      -DCMAKE_Fortran_FLAGS_RELWITHDEBINFO="-g -debug inline-debug-info -p -O3 -ip -qno-offload" \
      -DTARGET_MACHINE:STRING=shadow \
      -DCMAKE_INSTALL_PREFIX:STRING="$TC_INSTALL_PREFIX" \
      -DMODULEFILE_PREFIX:STRING="$TC_MODULEFILE_PREFIX" \
      -DCMAKE_MODULE_PATH:STRING="$CMAKE_MODULE_PATH"
  ;;
  *)
    echo "$@"
    eval "$@"
  ;;
esac

