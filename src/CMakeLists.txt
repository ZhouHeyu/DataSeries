#
# (c) Copyright 2008, Hewlett-Packard Development Company, LP
#
#  See the file named COPYING for license details
#
# cmake rules for the src directory

INCLUDE_DIRECTORIES(
	${DataSeries_SOURCE_DIR}/include # first to override older installed ones
	${DataSeries_SOURCE_DIR}/src

	${LINTEL_INCLUDE_DIR} # so local build overrides global install

        ${Boost_INCLUDE_DIRS}
        ${LIBXML2_INCLUDE_DIR}
	${CRYPTO_INCLUDE_DIR}
	${FULL_SRT_INCLUDE_DIR}
	${PCRE_INCLUDE_DIR}
	${PCAP_INCLUDE_DIR}
        ${ZLIB_INCLUDE_DIR}
)

ADD_DEFINITIONS(-D_FILE_OFFSET_BITS=64 -D_LARGE_FILES -I${CMAKE_CURRENT_SOURCE_DIR}/liblzf-1.6 
  	        -DDATASERIES_VERSION=\"\\"${DATASERIES_VERSION}\\"\")

IF ("${LINTEL_SYSTEM_TYPE}" STREQUAL "OpenBSD")
    ADD_DEFINITIONS(-pthread)
ENDIF ("${LINTEL_SYSTEM_TYPE}" STREQUAL "OpenBSD")
	

IF(CMAKE_COMPILER_IS_GNUCXX)
    ADD_DEFINITIONS(-Wall)
ENDIF(CMAKE_COMPILER_IS_GNUCXX)

IF("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    ADD_DEFINITIONS(-DDEBUG)
ENDIF("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")

SET(LIBDATASERIES_SOURCES
	base/DataSeriesSink.cpp
	base/DataSeriesSource.cpp
	base/Extent.cpp
	base/ExtentField.cpp
	base/ExtentSeries.cpp
	base/ExtentType.cpp
	base/GeneralField.cpp
	base/Int64TimeField.cpp
        base/RotatingFileSink.cpp
        base/SubExtentPointer.cpp
	process/commonargs.cpp
	module/DSExpr.cpp
	module/DSExprImpl.cpp
	module/DSExprParse.cpp
	module/DSExprScan.cpp
	module/DSStatGroupByModule.cpp
	module/DStoTextModule.cpp
	module/DataSeriesModule.cpp
        module/ExtentReleaseHack.cpp
	module/IndexSourceModule.cpp
	module/MinMaxIndexModule.cpp
	module/PrefetchBufferModule.cpp
	module/RowAnalysisModule.cpp
	module/SequenceModule.cpp
	module/TypeIndexModule.cpp
	liblzf-1.6/lzf_c.c
	liblzf-1.6/lzf_d.c 
)

################################## SUBDIRS

ADD_SUBDIRECTORY(process)
ADD_SUBDIRECTORY(analysis)
ADD_SUBDIRECTORY(BatchParallel)
ADD_SUBDIRECTORY(raw-collection)
ADD_SUBDIRECTORY(tests)
ADD_SUBDIRECTORY(perl-modules)
IF(BUILD_PAPER)
    ADD_SUBDIRECTORY(paper)
ENDIF(BUILD_PAPER)
IF(BUILD_EXAMPLE)
    ADD_SUBDIRECTORY(example)
ENDIF(BUILD_EXAMPLE)
IF(DATA_SERIES_SERVER_ENABLED)
  ADD_SUBDIRECTORY(server)
ENDIF(DATA_SERIES_SERVER_ENABLED)

################################## SCRIPTS

DATASERIES_INSTALL_CONFIG_PROGRAM(dataseries-config)

IF(PERL_XML_PARSER_ENABLED)
    DATASERIES_INSTALL_CONFIG_PROGRAM(dstypes2cxx)
ENDIF(PERL_XML_PARSER_ENABLED)

################################## MISC

ADD_CUSTOM_TARGET(rebuild_DSExpr
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/module/rebuild-dsexpr.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

################################## LIBRARY CONDITIONAL

IF(BZIP2_ENABLED)
    INCLUDE_DIRECTORIES(${BZIP2_INCLUDE_DIR})
    ADD_DEFINITIONS(-DDATASERIES_ENABLE_BZIP2=1)
ENDIF(BZIP2_ENABLED)

IF(LZO_ENABLED)
    INCLUDE_DIRECTORIES(${LZO_INCLUDE_DIR})
    ADD_DEFINITIONS(-DDATASERIES_ENABLE_LZO=1)
ENDIF(LZO_ENABLED)

IF(SNAPPY_ENABLED)
    INCLUDE_DIRECTORIES(${SNAPPY_INCLUDE_DIR})
    ADD_DEFINITIONS(-DDATASERIES_ENABLE_SNAPPY=1)
ENDIF(SNAPPY_ENABLED)

IF(LZ4_ENABLED)
    INCLUDE_DIRECTORIES(${LZ4_INCLUDE_DIR})
    ADD_DEFINITIONS(-DDATASERIES_ENABLE_LZ4=1)
ENDIF(LZ4_ENABLED)

IF(CRYPTO_ENABLED)
    LIST(APPEND LIBDATASERIES_SOURCES module/cryptutil.cpp)
    ADD_DEFINITIONS(-DDATASERIES_ENABLE_CRYPTO=1)
ENDIF(CRYPTO_ENABLED)

################################## LIBRARY

ADD_LIBRARY(DataSeries ${LIBRARY_TYPE} ${LIBDATASERIES_SOURCES})
SET_TARGET_PROPERTIES(DataSeries 
    PROPERTIES VERSION ${DATASERIES_VERSION} SOVERSION ${DATASERIES_ABI_VERSION})
TARGET_LINK_LIBRARIES(DataSeries ${LINTEL_LIBRARIES} ${LINTELPTHREAD_LIBRARIES}
                      ${LIBXML2_LIBRARIES} ${ZLIB_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT}
                      ${BOOST_THREAD_LIBRARIES})

################################## CONDITIONAL LIBRARY

IF(BZIP2_ENABLED)
    TARGET_LINK_LIBRARIES(DataSeries ${BZIP2_LIBRARIES})
ENDIF(BZIP2_ENABLED)

IF(LZO_ENABLED)
    TARGET_LINK_LIBRARIES(DataSeries ${LZO_LIBRARIES})
ENDIF(LZO_ENABLED)

IF(SNAPPY_ENABLED)
    TARGET_LINK_LIBRARIES(DataSeries ${SNAPPY_LIBRARIES})
ENDIF(SNAPPY_ENABLED)

IF(LZ4_ENABLED)
    TARGET_LINK_LIBRARIES(DataSeries ${LZ4_LIBRARIES})
ENDIF(LZ4_ENABLED)

IF(CRYPTO_ENABLED)
    TARGET_LINK_LIBRARIES(DataSeries ${CRYPTO_LIBRARIES})
ENDIF(CRYPTO_ENABLED)

################################## INSTALL

INSTALL(TARGETS DataSeries DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
