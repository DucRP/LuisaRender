add_library(luisa-render-ext INTERFACE)

add_subdirectory(cxxopts)
target_link_libraries(luisa-render-ext INTERFACE cxxopts::cxxopts)

find_package(assimp CONFIG)
if (NOT assimp_FOUND)
    set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
    set(ASSIMP_INJECT_DEBUG_POSTFIX OFF CACHE BOOL "" FORCE)
    set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_STL_IMPORTER ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_PLY_IMPORTER ON CACHE BOOL "" FORCE)
    set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
    add_subdirectory(assimp)
    set_target_properties(assimp PROPERTIES UNITY_BUILD OFF)
endif ()
target_link_libraries(luisa-render-ext INTERFACE assimp::assimp)

add_subdirectory(double-conversion)
set_target_properties(double-conversion PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
        UNITY_BUILD ON)
target_link_libraries(luisa-render-ext INTERFACE double-conversion)
