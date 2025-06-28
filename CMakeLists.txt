# Ajouter les sources de la bibliothèque
file(GLOB SOURCES "src/*.cpp")

# Créer la bibliothèque
add_library(strategies STATIC ${SOURCES})

# Configurer les chemins d'include
target_include_directories(strategies
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/include/Strategies
)

# Ajouter spdlog pour les headers qui l'utilisent
find_package(spdlog REQUIRED)
target_link_libraries(strategies PUBLIC spdlog::spdlog)