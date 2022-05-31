set(APP_SOURCES
)

add_executable(app
    app.cpp
    ${APP_SOURCES}
)


find_package(OpenSSL REQUIRED)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.10.5/json.tar.xz)
FetchContent_MakeAvailable(json)

FetchContent_Declare (httplib URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.10.7.tar.gz)
FetchContent_MakeAvailable(httplib)


target_include_directories(app
    PRIVATE
    ${SOURCE_DIR}
    ${OPENSSL_INCLUDE_DIR}
)


target_link_libraries(app
    PRIVATE
    nlohmann_json::nlohmann_json
    httplib::httplib
    ${OPENSSL_SSL_LIBRARY}
    ${OPENSSL_CRYPTO_LIBRARY}
)


target_compile_options(app 
    PRIVATE

    #FEATURES
    -std=c++20
    -fcoroutines  
    -fconcepts 
    -fconcepts-diagnostics-depth=2 
    # -O2 
    -ggdb

    # WARNINGS
    -Wpedantic  
    -Wall 
    -Wextra 
    -Werror 
    -Winit-self 
    -Wcast-align 
    -Wcast-qual 
    -Wdisabled-optimization 
    -Wlogical-op 
    -Wmissing-include-dirs 
    -Woverloaded-virtual 
    -Wstrict-null-sentinel 
    -Wctor-dtor-privacy 
    -Wfloat-equal 
    -Wredundant-decls   
    -Wsign-conversion 
    -Wsign-promo  
    -Wstrict-overflow=5 
    -Wswitch-default 
    -Wno-missing-field-initializers 
    -Wno-switch-default 
    -Wno-cast-qual 
    -Wno-strict-overflow 
)


target_link_options(app
    PRIVATE
)


target_compile_definitions(app 
    PRIVATE 
)