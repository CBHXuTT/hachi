cmake_minimum_required(VERSION 3.4.1)
if(UNIX)
  project(hachi C CXX)
else()
  project(hachi CXX)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fno-strict-aliasing")

function(include_sub_directories_recursively root_dir)
    if (IS_DIRECTORY ${root_dir})               # 当前路径是一个目录吗，是的话就加入到包含目录
        # message("include dir: " ${root_dir})
        include_directories(${root_dir})
    endif()

    file(GLOB ALL_SUB RELATIVE ${root_dir} ${root_dir}/*) # 获得当前目录下的所有文件，让如ALL_SUB列表中
    foreach(sub ${ALL_SUB})
        if (IS_DIRECTORY ${root_dir}/${sub})
            include_sub_directories_recursively(${root_dir}/${sub}) # 对子目录递归调用，包含
        endif()
    endforeach()
endfunction()

# include_sub_directories_recursively(${CMAKE_SOURCE_DIR}/src/main/jni/cpprestsdk)
# include_sub_directories_recursively(${CMAKE_SOURCE_DIR}/src/main/jni/include)
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/libuv/include)


file(GLOB SOURCES_FILES ${CMAKE_CURRENT_SOURCE_DIR}/*.cc)
if(WIN32)
  set(LINK_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/libuv/libs/win/x86/libuvd.lib)
  set(LINK_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/libuv/libs/win/x86/libuv.lib)

  file(GLOB APP_SOURCES test/*.cc)
  foreach( testsourcefile ${APP_SOURCES} )
      # I used a simple string replace, to cut off .cpp.
      # string( REPLACE ".cc" "" testname ${testsourcefile} )
      string(REGEX REPLACE ".+/(.+)\\..*" "\\1" testname ${testsourcefile})
      add_executable( ${testname} ${SOURCES_FILES} ${testsourcefile} )
      # Make sure YourLib is linked to each app
      target_link_libraries( ${testname} debug ${LINK_DEBUG} optimized ${LINK_RELEASE} )
  endforeach( testsourcefile ${APP_SOURCES} )

  # add_executable(test_ipc_server  ipc_server.cc test/test_ipc_server.cc)
  # target_link_libraries( 
  #   test_ipc_server
  #   debug ${LINK_DEBUG} optimized ${LINK_RELEASE})
  
  # add_executable(test_ipc_client  ipc_client.cc test/test_ipc_client.cc) 
  # target_link_libraries( 
  #   test_ipc_client
  #   debug ${LINK_DEBUG} optimized ${LINK_RELEASE})

endif()

