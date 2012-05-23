# - Find Magnum plugins
#
# Basic usage:
#
#  find_package(MagnumPlugins [REQUIRED])
#
# This command tries to find Magnum plugins and then defines:
#
#  MAGNUMPLUGINS_FOUND          - Whether Magnum plugins were found
#  MAGNUMPLUGINS_INCLUDE_DIR    - Include dir for Magnum plugins
#
# This command won't actually find any specific plugins, you have to specify
# them explicitly as components. Main dependency is Magnum library, additional
# dependencies are specified by the plugins. The plugins are:
#
#  ColladaImporter - COLLADA importer (depends on Qt4 package)
#  TgaImporter     - TGA importer
#
# Example usage with specifying additional components is:
#
#  find_package(MagnumPlugins [REQUIRED|COMPONENTS] ColladaImporter)
#
# For each plugin is then defined:
#
#  MAGNUMPLUGINS_*_FOUND    - Whether the plugin module and includes were found
#

# Dependencies
find_package(Magnum REQUIRED)

# Additional components
foreach(component ${MAGNUM_FIND_COMPONENTS})
    string(TOUPPER ${component} _COMPONENT)

    set(_MAGNUM_${_COMPONENT}_INCLUDE_PATH_SUFFIX ${component})
    set(_MAGNUM_${_COMPONENT}_INCLUDE_PATH_NAMES ${component}.h)

    # Importer plugins
    if(${component} MATCHES .+Importer)

        # Modules
        find_library(_MAGNUMPLUGINS_${_COMPONENT}_MODULE ${component}
            PATHS ${MAGNUM_PLUGINS_IMPORTER_DIR}
        )

        # ColladaImporter dependencies
        if(${component} EQUAL ColladaImporter)
            if(MAGNUMPLUGINS_FIND_REQUIRED_${component})
                find_package(Qt4 REQUIRED)
            else()
                find_package(Qt4)
                if(NOT QT4_FOUND)
                    unset(MAGNUMPLUGINS_${_COMPONENT}_MODULE)
                endif()
            endif()
        endif()
    endif()

    # Try to find the includes
    if(${_MAGNUMPLUGINS_${_COMPONENT}_INCLUDE_PATH_NAMES})
        find_path(_MAGNUMPLUGINS_${_COMPONENT}_INCLUDE_DIR
            NAMES ${_MAGNUMPLUGINS_${_COMPONENT}_INCLUDE_PATH_NAMES}
            PATH_SUFFIXES ${_MAGNUMPLUGINS_${_COMPONENT}_INCLUDE_PATH_SUFFIX}
        )
    endif()

    # Decide if the library was found
    if(${_MAGNUMPLUGINS_${_COMPONENT}_MODULE} AND ${_MAGNUMPLUGINS_${_COMPONENT}_INCLUDE_DIR})
        set(MAGNUMPLUGINS_${_COMPONENT}_FOUND TRUE)
    else()
        set(MAGNUMPLUGINS_${_COMPONENT}_FOUND FALSE)
    endif()
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MagnumPlugins
    REQUIRED_VARS MAGNUMPLUGINS_INCLUDE_DIR
    HANDLE_COMPONENTS
)
