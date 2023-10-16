cmake_minimum_required (VERSION 3.11.0)

set(BUGGER_VERSION_MAJOR 0)
set(BUGGER_VERSION_MINOR 1)
set(BUGGER_VERSION_PATCH 0)

project(exact VERSION ${BUGGER_VERSION_MAJOR}.${BUGGER_VERSION_MINOR}.${BUGGER_VERSION_PATCH}.0)

set(BOOST_MIN_VERSION 1.65)
if(APPLE)
   set(BOOST_MIN_VERSION 1.81)
endif()
set(CMAKE_CXX_STANDARD 14)
set(CXX_STANDARD_REQUIRED ON)
# set(CMAKE_CXX_EXTENSIONS OFF) # use -std=c++... instead of -std=gnu++...
# required for gcc if set(CMAKE_CXX_EXTENSIONS OFF) is used
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fext-numeric-literals")

# if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-psabi")
# endif()
if(MSVC)
   add_compile_options(/permissive)
endif()

include(GNUInstallDirs)

# path to e.g. findGMP module
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules/)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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
   file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/src/exact/Config.hpp githash_define REGEX "define BUGGER_GITHASH .*")
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

add_library(exact INTERFACE
        src/exact/interfaces/ViprInterface.hpp)
target_include_directories(exact INTERFACE
   $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
   $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
   $<INSTALL_INTERFACE:include>
   ${Boost_INCLUDE_DIRS}
   ${GMP_INCLUDE_DIRS}
   ${PAPILO_IMPORTED_TARGETS}
   )

option(TBB "should TBB be linked if found" ON)
option(PAPILO "should papilo library be linked" ON)

set(EXACT_TBB on)
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

target_link_libraries(exact
        INTERFACE TBB::tbb Threads::Threads $<$<PLATFORM_ID:Linux>:rt>)

find_package(Threads REQUIRED)


target_link_libraries(exact
        INTERFACE  ${GMP_LIBRARIES} ${GLOP_LIBRARIES} ${PAPILO_IMPORTED_TARGETS})

# on raspberry pi, we need to link libatomic, as libtbbmalloc_proxy depends on it
if((CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "armv7l") AND (CMAKE_SYSTEM_NAME STREQUAL "Linux"))
    target_link_libraries(exact INTERFACE atomic)
endif()

include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    ${CMAKE_BINARY_DIR}/exact-config-version.cmake
   VERSION ${BUGGER_VERSION_MAJOR}.${BUGGER_VERSION_MINOR}.${BUGGER_VERSION_PATCH}
   COMPATIBILITY SameMajorVersion
)

if(MSVC)
   target_compile_definitions(exact INTERFACE BOOST_ALL_NO_LIB)
   target_compile_options(exact INTERFACE /bigobj)
   target_compile_options(exact
      INTERFACE ${TBB_CXX_STD_FLAG})
else()
   target_compile_options(exact
      INTERFACE ${TBB_CXX_STD_FLAG} -Wno-shadow)
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
set(CMAKE_REQUIRED_INCLUDES ${PROJECT_SOURCE_DIR}/src/exact/external)
check_cxx_source_compiles(
   "#include \"ska/bytell_hash_map.hpp\"
   int main() { ska::bytell_hash_map<int,int> hashmap; (void)hashmap; return 0; }"
   EXACT_BYTELL_HASHMAP_WORKS )

if( EXACT_BYTELL_HASHMAP_WORKS )
   set(EXACT_USE_STANDARD_HASHMAP 0)
else()
   set(EXACT_USE_STANDARD_HASHMAP 1)
endif()

add_library(bugger-core STATIC
        src/exact/data/VariableDomains.cpp
        src/exact/data/SparseStorage.cpp
        src/exact/data/ConstraintMatrix.cpp
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


target_link_libraries(bugger-core exact)
target_compile_definitions(bugger-core PRIVATE BUGGER_USE_EXTERN_TEMPLATES)

if(NOT EXACT_NO_BINARIES)
   add_subdirectory(binaries)
endif()

configure_file("${PROJECT_SOURCE_DIR}/src/exact/CMakeConfig.hpp.in"
               "${PROJECT_BINARY_DIR}/exact/CMakeConfig.hpp")

# add tests
enable_testing()
#add_subdirectory(${PROJECT_SOURCE_DIR}/test)

# install the header files of exact
install(FILES
     ${PROJECT_BINARY_DIR}/exact/CMakeConfig.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/Config.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/exact)

install(FILES
     ${CMAKE_BINARY_DIR}/exact-config-version.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/exact)

install(FILES
        ${PROJECT_SOURCE_DIR}/src/exact/data/ConstraintMatrix.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/data/MatrixBuffer.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/data/Objective.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/data/ExactOptions.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/data/Problem.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/data/ProblemBuilder.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/data/RowFlags.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/data/SingleRow.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/data/Solution.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/data/SparseStorage.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/data/VariableDomains.hpp
   DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/exact/data)

install(FILES
        ${PROJECT_SOURCE_DIR}/src/exact/interfaces/ScipInterface.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/interfaces/ViprInterface.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/exact/interfaces)

install(FILES
        ${PROJECT_SOURCE_DIR}/src/exact/io/Message.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/io/MpsParser.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/io/MpsWriter.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/io/SolParser.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/io/SolWriter.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/exact/io)

install(FILES
        ${PROJECT_SOURCE_DIR}/src/exact/misc/Alloc.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/misc/Array.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/misc/compress_vector.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/misc/Flags.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/misc/fmt.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/misc/Hash.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/misc/MultiPrecision.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/misc/Num.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/misc/NumericalStatistics.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/misc/OptionsParser.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/misc/VersionLogger.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/misc/ParameterSet.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/misc/StableSum.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/misc/String.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/misc/tbb.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/misc/Timer.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/misc/Validation.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/misc/Vec.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/misc/VectorUtils.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/misc/Wrappers.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/exact/misc)


if((DEFINED Boost_VERSION) AND (Boost_VERSION VERSION_LESS 107000))
   install(FILES
           ${PROJECT_SOURCE_DIR}/src/exact/misc/extended_euclidean.hpp
           DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/exact/misc)
endif()


install(FILES
        ${PROJECT_SOURCE_DIR}/src/exact/external/fmt/chrono.h
        ${PROJECT_SOURCE_DIR}/src/exact/external/fmt/color.h
        ${PROJECT_SOURCE_DIR}/src/exact/external/fmt/compile.h
        ${PROJECT_SOURCE_DIR}/src/exact/external/fmt/core.h
        ${PROJECT_SOURCE_DIR}/src/exact/external/fmt/format.h
        ${PROJECT_SOURCE_DIR}/src/exact/external/fmt/format-inl.h
        ${PROJECT_SOURCE_DIR}/src/exact/external/fmt/locale.h
        ${PROJECT_SOURCE_DIR}/src/exact/external/fmt/ostream.h
        ${PROJECT_SOURCE_DIR}/src/exact/external/fmt/posix.h
        ${PROJECT_SOURCE_DIR}/src/exact/external/fmt/printf.h
        ${PROJECT_SOURCE_DIR}/src/exact/external/fmt/ranges.h
        ${PROJECT_SOURCE_DIR}/src/exact/external/fmt/format.cc
        ${PROJECT_SOURCE_DIR}/src/exact/external/fmt/posix.cc
 DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/exact/external/fmt)

install(FILES
        ${PROJECT_SOURCE_DIR}/src/exact/external/pdqsort/pdqsort.h
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/exact/external/pdqsort)

install(FILES
        ${PROJECT_SOURCE_DIR}/src/exact/external/ska/bytell_hash_map.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/external/ska/flat_hash_map.hpp
        ${PROJECT_SOURCE_DIR}/src/exact/external/ska/unordered_map.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/exact/external/ska)

# configure the config file for the build tree
set(TBB_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules)
configure_file(${PROJECT_SOURCE_DIR}/exact-config.cmake.in
  "${CMAKE_BINARY_DIR}/exact-config.cmake" @ONLY)

# configure and install config file for installation
set(TBB_MODULE_PATH "\${CMAKE_CURRENT_LIST_DIR}")
configure_file(${PROJECT_SOURCE_DIR}/exact-config.cmake.in
    "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/exact-config.cmake" @ONLY)

# install dependencies
install(FILES ${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/exact-config.cmake
   DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/exact)

install(FILES ${PROJECT_SOURCE_DIR}/cmake/Modules/FindTBB.cmake
   DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/exact)
if(INSTALL_TBB AND CMAKE_SYSTEM_NAME STREQUAL "Windows")
   if(EXISTS "${TBB_tbb_DLL}")
      install(FILES "${TBB_tbb_DLL}" DESTINATION ${CMAKE_INSTALL_BINDIR})
      install(FILES "${TBB_tbb_DLL}" DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/exact)
   endif()
   if(EXISTS "${TBB_tbbmalloc_DLL}")
      install(FILES "${TBB_tbbmalloc_DLL}" DESTINATION ${CMAKE_INSTALL_BINDIR})
   endif()
endif()

# export targets for build-tree linking
export(TARGETS exact bugger-core FILE "${CMAKE_BINARY_DIR}/bugger-targets.cmake")

if(EXISTS ${CMAKE_BINARY_DIR}/_deps)
   install(DIRECTORY ${CMAKE_BINARY_DIR}/_deps DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/exact/)
endif()

# install targets
install(TARGETS exact bugger-core EXPORT bugger-targets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(EXPORT bugger-targets DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/exact)

if(TARGET bugger-executable)
   install(TARGETS bugger-executable ARCHIVE DESTINATION ${CMAKE_INSTALL_BINDIR} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()