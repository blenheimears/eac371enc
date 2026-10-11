cmake_minimum_required(VERSION 3.16)
project(eac371enc VERSION 0.1.6 LANGUAGES C)

option(EAC371_ENABLE_SNDFILE_INPUT "Enable libsndfile input support" ON)
option(EAC371_BUILD_TESTS "Build/register eac371enc tests" OFF)

set(EAC371_CODEC_SOURCES
    src/codec/ac3.c
    src/codec/ac3dsp.c
    src/codec/ac3enc.c
    src/codec/ac3enc_float.c
    src/codec/ac3tab.c
    src/codec/audiodsp.c
    src/codec/codec_api.c
    src/codec/eac3_data.c
    src/codec/eac3enc.c
    src/codec/kbdwin.c
    src/codec/mdct_standalone.c
    src/codec/standalone_compat.c)

add_executable(eac371enc src/eac371enc.c src/resampler.c ${EAC371_CODEC_SOURCES})
target_include_directories(eac371enc PRIVATE src src/codec)
target_compile_features(eac371enc PRIVATE c_std_11)
if(MSVC)
  target_compile_options(eac371enc PRIVATE /W4)
else()
  target_compile_options(eac371enc PRIVATE -Wall -Wextra -Wpedantic)
  target_link_libraries(eac371enc PRIVATE m)
endif()

if(EAC371_ENABLE_SNDFILE_INPUT)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SNDFILE REQUIRED IMPORTED_TARGET sndfile)
  target_link_libraries(eac371enc PRIVATE PkgConfig::SNDFILE)
  target_compile_definitions(eac371enc PRIVATE EAC371_HAVE_SNDFILE=1)
else()
  target_compile_definitions(eac371enc PRIVATE EAC371_HAVE_SNDFILE=0)
endif()

install(TARGETS eac371enc RUNTIME DESTINATION bin)

if(EAC371_BUILD_TESTS)
  enable_testing()
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_test(NAME eac371enc-channel-layouts COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/layout_regression.py $<TARGET_FILE:eac371enc>)
  if(EAC371_ENABLE_SNDFILE_INPUT)
    add_test(NAME eac371enc-smoke COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke.sh $<TARGET_FILE:eac371enc>)
  else()
    add_test(NAME eac371enc-raw-smoke COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/raw_smoke.py $<TARGET_FILE:eac371enc>)
  endif()
endif()
