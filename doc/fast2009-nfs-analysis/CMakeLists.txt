INCLUDE(LintelCMakeUtil)

IF(BUILD_FAST_2009)
    SET(fast2009-nfs-analysis_LINTEL_LATEX_ARGS --error-on-warnings)

    LINTEL_LATEX(fast2009-nfs-analysis)
    INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/fast2009-nfs-analysis.pdf
            DESTINATION ${CMAKE_INSTALL_PREFIX}/share/doc/DataSeries)
ENDIF(BUILD_FAST_2009)
