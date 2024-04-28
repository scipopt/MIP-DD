cmake_minimum_required (VERSION 3.11.0)

set(BUGGER_VERSION_MAJOR 1)
set(BUGGER_VERSION_MINOR 1)
set(BUGGER_VERSION_PATCH 0)

SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

project(bugger VERSION ${BUGGER_VERSION_MAJOR}.${BUGGER_VERSION_MINOR}.${BUGGER_VERSION_PATCH})

set(CMAKE_CXX_STANDARD 14)

set(BOOST_MIN_VERSION 1.65)
if(APPLE)
   set(BOOST_MIN_VERSION 1.81)
endif()

include(GNUInstallDirs)

# path to e.g. findGMP module
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules/)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# disable fused floating point contraction to enhance reproducibility across compilers and architectures
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    add_compile_options(/fp:precise)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Intel")
    add_compile_options(-fp-model=precise)
elseif((CMAKE_CXX_COMPILER_ID MATCHES "GNU") OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
    add_compile_options(-ffp-contract=off)
endif()

option(GMP "should gmp be linked" ON)
option(INSTALL_TBB "should the TBB library be installed" OFF)

# make 'Release' the default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Boost ${BOOST_MIN_VERSION} COMPONENTS program_options REQUIRED)

find_program(GIT git)

if((GIT) AND (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git))
   execute_process(
      COMMAND ${GIT} rev-parse --short HEAD
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      OUTPUT_VARIABLE BUGGER_GITHASH OUTPUT_STRIP_TRAILING_WHITESPACE)
else()
   file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/src/bugger/Config.hpp githash_define REGEX "define BUGGER_GITHASH .*")
   if(githash_define)
      string(REGEX MATCH "\\\"(.*)\\\"" _matched ${githash_define})
      if(_matched)
         set(BUGGER_GITHASH ${CMAKE_MATCH_1})
      endif()
   endif()
endif()

if(BUGGER_GITHASH)
   message(STATUS "Git hash: ${BUGGER_GITHASH}")
   set(BUGGER_GITHASH_AVAILABLE 1)
else()
   set(BUGGER_GITHASH_AVAILABLE 0)
endif()

add_library(bugger INTERFACE )
target_include_directories(bugger INTERFACE
   $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
   $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
   $<INSTALL_INTERFACE:include>
   ${Boost_INCLUDE_DIRS}
   ${GMP_INCLUDE_DIRS}
   )

option(TBB "should TBB be linked if found" OFF)

set(BUGGER_TBB off)

# on windows we don't want to build tbb
if(WIN32)
   find_package(TBB 2018 COMPONENTS tbb tbbmalloc REQUIRED)
else()
   # Utilities to automatically download missing dependencies
   include(cmake/Dependencies.cmake)

   find_or_download_package(
      NAME TBB
      VERSION 2018
     URL https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2021.4.0.tar.gz
      COMPONENTS tbb tbbmalloc
      URL_HASH SHA256=021796c7845e155e616f5ecda16daa606ebb4c6f90b996e5c08aebab7a8d3de3
      CONFIGURE_ARGS -DTBB_TEST=OFF -DTBB_EXAMPLES=OFF -DTBB4PY_BUILD=OFF)
endif()

target_link_libraries(bugger
        INTERFACE TBB::tbb Threads::Threads $<$<PLATFORM_ID:Linux>:rt>)

find_package(Threads REQUIRED)


target_link_libraries(bugger INTERFACE  ${GMP_LIBRARIES})

# on raspberry pi, we need to link libatomic, as libtbbmalloc_proxy depends on it
if((CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "armv7l") AND (CMAKE_SYSTEM_NAME STREQUAL "Linux"))
    target_link_libraries(bugger INTERFACE atomic)
endif()

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    ${CMAKE_BINARY_DIR}/bugger-config-version.cmake
   VERSION ${BUGGER_VERSION_MAJOR}.${BUGGER_VERSION_MINOR}.${BUGGER_VERSION_PATCH}
   COMPATIBILITY SameMajorVersion
)

if(MSVC)
   target_compile_definitions(bugger INTERFACE BOOST_ALL_NO_LIB)
   target_compile_options(bugger INTERFACE /bigobj)
   target_compile_options(bugger INTERFACE ${TBB_CXX_STD_FLAG})
else()
   target_compile_options(bugger INTERFACE ${TBB_CXX_STD_FLAG} -Wno-shadow)
endif()

include(CheckCXXSourceCompiles)
include(CheckCXXCompilerFlag)

if(NOT MSVC)
   check_cxx_compiler_flag(-std=c++14 HAVE_FLAG_STD_CXX14)
   if(HAVE_FLAG_STD_CXX14)
      set(CMAKE_REQUIRED_FLAGS "-std=c++14")
   else()
      check_cxx_compiler_flag(-std=c++1y HAVE_FLAG_STD_CXX1Y)
      if(HAVE_FLAG_STD_CXX1Y)
         set(CMAKE_REQUIRED_FLAGS "-std=c++1y")
      endif()
   endif()
endif()
set(CMAKE_REQUIRED_INCLUDES ${PROJECT_SOURCE_DIR}/src/bugger/external)
check_cxx_source_compiles(
   "#include \"ska/bytell_hash_map.hpp\"
   int main() { ska::bytell_hash_map<int,int> hashmap; (void)hashmap; return 0; }"
   BUGGER_BYTELL_HASHMAP_WORKS )

if( BUGGER_BYTELL_HASHMAP_WORKS )
   set(BUGGER_USE_STANDARD_HASHMAP 0)
else()
   set(BUGGER_USE_STANDARD_HASHMAP 1)
endif()

add_library(bugger-core STATIC
        src/bugger/data/VariableDomains.cpp
        src/bugger/data/SparseStorage.cpp
        src/bugger/data/ConstraintMatrix.cpp
)

#
# interface function for setting common library properties
#
function(setLibProperties targetname outputname)
    set_target_properties(${targetname} PROPERTIES
        OUTPUT_NAME ${outputname}
        MACOSX_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
endfunction(setLibProperties)
if(MSVC)
    setLibProperties(bugger-core "libbugger-core")
else()
    setLibProperties(bugger-core "bugger-core")
endif()

# define bugger arithmetic
set(BUGGER_ARITHMETIC d CACHE STRING "arithmetic for reductions, problems, and solutions (f: float, d: double, l: long double)")
if    ("float"      MATCHES ^${BUGGER_ARITHMETIC})
    target_compile_definitions(bugger-core PUBLIC BUGGER_FLOAT)
elseif("double"     MATCHES ^${BUGGER_ARITHMETIC})
    target_compile_definitions(bugger-core PUBLIC BUGGER_DOUBLE)
elseif("longdouble" MATCHES ^${BUGGER_ARITHMETIC})
    target_compile_definitions(bugger-core PUBLIC BUGGER_LONGDOUBLE)
else()
    message(FATAL_ERROR "Arithmetic not supported!")
endif()

find_package(Boost REQUIRED iostreams serialization program_options)
set(CMAKE_REQUIRED_INCLUDES ${Boost_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${Boost_LIBRARIES})
check_cxx_source_compiles(
   "#include <boost/iostreams/filter/gzip.hpp>
    int main() { auto decomp = boost::iostreams::gzip_decompressor(); (void)decomp; return 0; }"
   BUGGER_USE_BOOST_IOSTREAMS_WITH_ZLIB )
check_cxx_source_compiles(
   "#include <boost/iostreams/filter/bzip2.hpp>
    int main() { auto decomp = boost::iostreams::bzip2_decompressor(); (void)decomp; return 0; }"
   BUGGER_USE_BOOST_IOSTREAMS_WITH_BZIP2 )

target_link_libraries(bugger-core bugger ${Boost_LIBRARIES})
target_compile_definitions(bugger-core PRIVATE BUGGER_USE_EXTERN_TEMPLATES)

add_subdirectory(binaries)

configure_file("${PROJECT_SOURCE_DIR}/src/bugger/CMakeConfig.hpp.in"
               "${PROJECT_BINARY_DIR}/bugger/CMakeConfig.hpp")

# add tests
enable_testing()
#add_subdirectory(${PROJECT_SOURCE_DIR}/test)

# install the header files of bugger
install(FILES
     ${PROJECT_BINARY_DIR}/bugger/CMakeConfig.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/Config.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/bugger)

install(FILES
     ${CMAKE_BINARY_DIR}/bugger-config-version.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bugger)

install(FILES
        ${PROJECT_SOURCE_DIR}/src/bugger/data/BuggerRun.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/data/BuggerParameters.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/data/ConstraintMatrix.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/data/MatrixBuffer.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/data/Objective.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/data/Problem.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/data/ProblemBuilder.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/data/RowFlags.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/data/SingleRow.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/data/Solution.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/data/SolverSettings.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/data/SparseStorage.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/data/VariableDomains.hpp
   DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/bugger/data)

install(FILES
        ${PROJECT_SOURCE_DIR}/src/bugger/interfaces/BuggerStatus.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/interfaces/SolverStatus.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/interfaces/SolverInterface.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/bugger/interfaces)

install(FILES
        ${PROJECT_SOURCE_DIR}/src/bugger/io/Message.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/io/MpsParser.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/io/MpsWriter.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/io/SolParser.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/bugger/io)

install(FILES
        ${PROJECT_SOURCE_DIR}/src/bugger/misc/Alloc.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/misc/Array.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/misc/compress_vector.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/misc/Flags.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/misc/fmt.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/misc/Hash.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/misc/MultiPrecision.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/misc/Num.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/misc/NumericalStatistics.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/misc/OptionsParser.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/misc/VersionLogger.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/misc/ParameterSet.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/misc/StableSum.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/misc/String.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/misc/tbb.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/misc/Timer.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/misc/Vec.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/misc/VectorUtils.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/bugger/misc)

install(FILES
        ${PROJECT_SOURCE_DIR}/src/bugger/modules/BuggerModul.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/modules/VariableModul.hpp
   DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/bugger/modules)

install(FILES
        ${PROJECT_SOURCE_DIR}/src/bugger/external/fmt/chrono.h
        ${PROJECT_SOURCE_DIR}/src/bugger/external/fmt/color.h
        ${PROJECT_SOURCE_DIR}/src/bugger/external/fmt/compile.h
        ${PROJECT_SOURCE_DIR}/src/bugger/external/fmt/core.h
        ${PROJECT_SOURCE_DIR}/src/bugger/external/fmt/format-inl.h
        ${PROJECT_SOURCE_DIR}/src/bugger/external/fmt/format.h
        ${PROJECT_SOURCE_DIR}/src/bugger/external/fmt/locale.h
        ${PROJECT_SOURCE_DIR}/src/bugger/external/fmt/os.h
        ${PROJECT_SOURCE_DIR}/src/bugger/external/fmt/ostream.h
        ${PROJECT_SOURCE_DIR}/src/bugger/external/fmt/posix.h
        ${PROJECT_SOURCE_DIR}/src/bugger/external/fmt/printf.h
        ${PROJECT_SOURCE_DIR}/src/bugger/external/fmt/ranges.h
   DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/bugger/external/fmt)

install(FILES
        ${PROJECT_SOURCE_DIR}/src/bugger/external/pdqsort/pdqsort.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/bugger/external/pdqsort)

install(FILES
        ${PROJECT_SOURCE_DIR}/src/bugger/external/ska/bytell_hash_map.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/external/ska/flat_hash_map.hpp
        ${PROJECT_SOURCE_DIR}/src/bugger/external/ska/unordered_map.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/bugger/external/ska)

# configure the config file for the build tree
set(TBB_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules)
configure_file(${PROJECT_SOURCE_DIR}/bugger-config.cmake.in
  "${CMAKE_BINARY_DIR}/bugger-config.cmake" @ONLY)

# configure and install config file for installation
set(TBB_MODULE_PATH "\${CMAKE_CURRENT_LIST_DIR}")
configure_file(${PROJECT_SOURCE_DIR}/bugger-config.cmake.in
    "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/bugger-config.cmake" @ONLY)

# install dependencies
install(FILES ${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/bugger-config.cmake
   DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bugger)

install(FILES ${PROJECT_SOURCE_DIR}/cmake/Modules/FindTBB.cmake
   DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bugger)
if(INSTALL_TBB AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
   if(EXISTS "${TBB_tbb_DLL}")
      install(FILES "${TBB_tbb_DLL}" DESTINATION ${CMAKE_INSTALL_BINDIR})
      install(FILES "${TBB_tbb_DLL}" DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bugger)
   endif()
   if(EXISTS "${TBB_tbbmalloc_DLL}")
      install(FILES "${TBB_tbbmalloc_DLL}" DESTINATION ${CMAKE_INSTALL_BINDIR})
   endif()
endif()

# export targets for build-tree linking
export(TARGETS bugger bugger-core FILE "${CMAKE_BINARY_DIR}/bugger-targets.cmake")

if(EXISTS ${CMAKE_BINARY_DIR}/_deps)
   install(DIRECTORY ${CMAKE_BINARY_DIR}/_deps DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bugger/)
endif()

# install targets
install(TARGETS bugger bugger-core EXPORT bugger-targets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(EXPORT bugger-targets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/bugger)

if(TARGET bugger-executable)
   install(TARGETS bugger-executable ARCHIVE DESTINATION ${CMAKE_INSTALL_BINDIR} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
