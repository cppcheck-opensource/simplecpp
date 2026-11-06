#!/usr/bin/env bash

if [ -z "$SIMPLECPP_PATH" ]; then
  SIMPLECPP_PATH=.
fi

if [ -n "$VALGRIND_TOOL" ]; then
  if [ "$VALGRIND_TOOL" = "memcheck" ]; then
    VALGRIND_OPTS="--error-limit=yes --leak-check=full --num-callers=50 --show-reachable=yes --track-origins=yes --gen-suppressions=all --error-exitcode=42"
  elif [ "$VALGRIND_TOOL" = "callgrind" ]; then
    VALGRIND_OPTS="--tool=callgrind"
  else
    echo "unsupported valgrind tool '$VALGRIND_TOOL'"
    exit 1
  fi
  VALGRIND_CMD="valgrind --tool=$VALGRIND_TOOL --log-fd=9 $VALGRIND_OPTS"
  VALGRIND_REDIRECT="valgrind_$VALGRIND_TOOL.log"
else
  VALGRIND_CMD=
  VALGRIND_REDIRECT="/dev/null"
fi

output=$($VALGRIND_CMD ./simplecpp "$SIMPLECPP_PATH/simplecpp.cpp" -e -f 2>&1 9> "$VALGRIND_REDIRECT")
ec=$?
cat "$VALGRIND_REDIRECT"
errors=$(echo "$output" | grep -v 'Header not found: <')
if [ $ec -ne 0 ]; then
  # only fail if we got errors which do not refer to missing system includes
  if [ ! -z "$errors" ]; then
    exit $ec
  fi
fi

if [ -z "$CXX" ]; then
  exit 0
fi

cxx_type=$($CXX --version | head -1 | cut -d' ' -f1)
if [ "$cxx_type" = "Ubuntu" ] || [ "$cxx_type" = "Debian" ]; then
  cxx_type=$($CXX --version | head -1 | cut -d' ' -f2)
fi

# TODO: generate defines from compiler
if [ "$cxx_type" = "g++" ] || [ "$cxx_type" = "g++.exe" ]; then
  defs=
  defs="$defs -D__GNUC__"
  defs="$defs -D__STDC__"
  defs="$defs -D__x86_64__"
  defs="$defs -D__STDC_HOSTED__"
  defs="$defs -D__CHAR_BIT__=8"
  if [ "${MSYSTEM}" = "MINGW32" ] || [ "${MSYSTEM}" = "MINGW64" ]; then
    defs="$defs -D_WIN32"
  fi
  defs="$defs -D__has_builtin(x)=(1)"
  defs="$defs -D__has_cpp_attribute(x)=(1)"
  defs="$defs -D__has_attribute(x)=(1)"
  defs="$defs -Ddefined(x)=(0)"

  inc=
  while read line
  do
    inc="$inc -I$line"
  done <<< "$($CXX -x c++ -v -c -S - 2>&1 < /dev/null | grep -e'^ [/A-Z]' | grep -v /cc1plus)"
elif [ "$cxx_type" = "clang" ]; then
  # libstdc++
  defs=
  defs="$defs -D__x86_64__"
  defs="$defs -D__STDC_HOSTED__"
  defs="$defs -D__CHAR_BIT__=8"
  defs="$defs -D__BYTE_ORDER__=1234"
  defs="$defs -D__SIZEOF_SIZE_T__=8"
  if [ "${MSYSTEM}" = "MINGW32" ] || [ "${MSYSTEM}" = "MINGW64" ] || [ "${MSYSTEM}" = "CLANG64" ]; then
    defs="$defs -D_WIN32"
  fi
  defs="$defs -D__has_builtin(x)=(1)"
  defs="$defs -D__has_cpp_attribute(x)=(1)"
  defs="$defs -D__has_feature(x)=(1)"
  defs="$defs -D__has_include_next(x)=(1)"
  defs="$defs -D__has_attribute(x)=(0)"
  defs="$defs -D__building_module(x)=(0)"
  defs="$defs -D__has_extension(x)=(1)"
  defs="$defs -Ddefined(x)=(0)"

  inc=
  while read line
  do
    inc="$inc -I$line"
  done <<< "$($CXX -x c++ -v -c -S - 2>&1 < /dev/null | grep -e'^ [/A-Z]')"

  # TODO: enable
  # libc++
  #defs=
  #defs="$defs -D__x86_64__"
  #defs="$defs -D__linux__"
  #defs="$defs -D__SIZEOF_SIZE_T__=8"
  #defs="$defs -D__has_include_next(x)=(0)"
  #defs="$defs -D__has_builtin(x)=(1)"
  #defs="$defs -D__has_feature(x)=(1)"

  #inc=
  #while read line
  #do
  #  inc="$inc -I$line"
  #done <<< "$($CXX -x c++ -stdlib=libc++ -v -c -S - 2>&1 < /dev/null | grep -e'^ [/A-Z]')"
elif [ "$cxx_type" = "Apple" ]; then
  defs=
  defs="$defs -D__BYTE_ORDER__"
  defs="$defs -D__APPLE__"
  defs="$defs -D__GNUC__=15"
  defs="$defs -D__x86_64__"
  defs="$defs -D__SIZEOF_SIZE_T__=8"
  defs="$defs -D__LITTLE_ENDIAN__"
  defs="$defs -D__has_feature(x)=(0)"
  defs="$defs -D__has_extension(x)=(1)"
  defs="$defs -D__has_attribute(x)=(0)"
  defs="$defs -D__has_cpp_attribute(x)=(0)"
  defs="$defs -D__has_include_next(x)=(0)"
  defs="$defs -D__has_builtin(x)=(1)"
  defs="$defs -D__is_target_os(x)=(0)"
  defs="$defs -D__is_target_arch(x)=(0)"
  defs="$defs -D__is_target_vendor(x)=(0)"
  defs="$defs -D__is_target_environment(x)=(0)"
  defs="$defs -D__is_target_variant_os(x)=(0)"
  defs="$defs -D__is_target_variant_environment(x)=(0)"

  inc=
  while read line
  do
    inc="$inc -I$line"
    # TODO: pass the framework path as such when possible
  done <<< "$($CXX -x c++ -v -c -S - 2>&1 < /dev/null | grep -e'^ [/A-Z]' | sed 's/ (framework directory)//g')"
  echo $inc
else
  echo "unknown compiler '$cxx_type'"
  exit 1
fi

# run with -std=gnuc++* so __has_include(...) is available
$VALGRIND_CMD ./simplecpp "$SIMPLECPP_PATH/simplecpp.cpp" -e -f -std=gnu++11 $defs $inc 9> "$VALGRIND_REDIRECT"
ec=$?
cat "$VALGRIND_REDIRECT"
if [ $ec -ne 0 ]; then
  exit $ec
fi
