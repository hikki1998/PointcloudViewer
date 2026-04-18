las_viewer_add_app_sources(main.cpp)

if(WIN32)
    las_viewer_add_app_sources(app_icon.rc)
endif()

add_subdirectory(crs)
add_subdirectory(capture)
add_subdirectory(domain)
add_subdirectory(gui)
add_subdirectory(logging)
add_subdirectory(osg)
add_subdirectory(pointcloud)
add_subdirectory(route)
