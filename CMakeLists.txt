cmake_minimum_required(VERSION 3.15)
project(nebula_pty)

# Spécifier la version de C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Inclure Node.js N-API
include_directories(${CMAKE_JS_INC})

# Ajouter les fichiers source
file(GLOB SOURCE_FILES "*.cc" "*.h")

# Créer la bibliothèque partagée
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${CMAKE_JS_SRC})

# Définir les options de compilation
if(MSVC)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        UNICODE
        _UNICODE
        WIN32
        _WINDOWS
        NODE_ADDON_API_CPP_EXCEPTIONS
    )
endif()

# Lier avec les bibliothèques nécessaires
target_link_libraries(${PROJECT_NAME} ${CMAKE_JS_LIB})

# Sous Windows, ajouter les bibliothèques Windows spécifiques
if(WIN32)
    target_link_libraries(${PROJECT_NAME}
        kernel32
        user32
        gdi32
        winspool
        shell32
        ole32
        oleaut32
        uuid
        comdlg32
        advapi32
    )
endif()

# Définir le préfixe de sortie
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")

# Si ce n'est pas Windows, ajouter -fPIC
if(NOT WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()