cmake_minimum_required(VERSION 3.6)
project(bruinbase)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

find_package(BISON)
find_package(FLEX)

set(FLEX_INCLUDE_DIRS ${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR})


BISON_TARGET(SqlParser SqlParser.y ${CMAKE_CURRENT_BINARY_DIR}/SqlParser.tab.cpp COMPILE_FLAGS "-d -psql")
FLEX_TARGET(SqlScanner SqlParser.l  ${CMAKE_CURRENT_BINARY_DIR}/lex.sql.cpp COMPILE_FLAGS "-Psql")

ADD_FLEX_BISON_DEPENDENCY(SqlScanner SqlParser)

include_directories(${CMAKE_CURRENT_BINARY_DIR})


set(SOURCE_FILES
    Bruinbase.h
    BTreeIndex.cc
    BTreeIndex.h
    BTreeNode.cc
    BTreeNode.h
    main.cc
    PageFile.cc
    PageFile.h
    RecordFile.cc
    RecordFile.h
    SqlEngine.cc
    SqlEngine.h)

set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(bruinbase ${SOURCE_FILES} ${BISON_SqlParser_OUTPUTS} ${FLEX_SqlScanner_OUTPUTS})