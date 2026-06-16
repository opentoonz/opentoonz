include(FetchContent)

# ---- spdlog (logging) ----
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.15.2
    SYSTEM
)
FetchContent_MakeAvailable(spdlog)

# ---- nlohmann/json (JSON) ----
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    SYSTEM
)
FetchContent_MakeAvailable(json)

# ---- Google Test (testing) ----
if(BUILD_TESTS)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.16.0
        SYSTEM
    )
    # Prevent gtest from installing itself
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()
