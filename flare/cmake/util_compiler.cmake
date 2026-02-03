include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

function(ADD_CHECK_C_COMPILER_FLAG
    _CFLAGS
    _CACHE_VAR
    _FLAG
    )

    CHECK_C_COMPILER_FLAG("${_FLAG}" "${_CACHE_VAR}")
    if(${_CACHE_VAR})
        # message(STATUS "Using CFLAG: ${_FLAG}")
        set(${_CFLAGS} "${${_CFLAGS}} ${_FLAG}" PARENT_SCOPE)
    else()
        message(STATUS "Unsupported CFLAG: ${_FLAG}")
    endif()
endfunction()

function(ADD_CHECK_CXX_COMPILER_FLAG
    _CXXFLAGS
    _CACHE_VAR
    _FLAG
    )

    CHECK_CXX_COMPILER_FLAG("${_FLAG}" "${_CACHE_VAR}")
    if(${_CACHE_VAR})
        # message(STATUS "Using CXXFLAG: ${_FLAG}")
        set(${_CXXFLAGS} "${${_CXXFLAGS}} ${_FLAG}" PARENT_SCOPE)
    else()
        message(STATUS "Unsupported CXXFLAG: ${_FLAG}")
    endif()
endfunction()

