# Search supplied hint directories first if supplied.
set(QPOASES_ROOT_DIR "$ENV{QPOASES_ROOT_DIR}" CACHE PATH "QPOASES root directory.")
if(QPOASES_ROOT_DIR)
 message("Looking for QPOASES in ${QPOASES_ROOT_DIR}")
else(QPOASES_ROOT_DIR)
 message("QPOASES_ROOT_DIR not provided.")
endif(QPOASES_ROOT_DIR)

find_path(QPOASES_INCLUDE_DIR
  NAMES qpOASES.hpp
  HINTS /usr/local/include
  HINTS /usr/include
  HINTS ${QPOASES_ROOT_DIR}/include
)


find_library(QPOASES_LIBRARY 
  qpOASES
  HINTS ${QPOASES_ROOT_DIR}/build/libs
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(QPOASES DEFAULT_MSG QPOASES_LIBRARY QPOASES_INCLUDE_DIR)

if(QPOASES_FOUND)  
  message("—- Found QPOASES include under ${QPOASES_INCLUDE_DIR}")
    set(QPOASES_INCLUDE_DIRS ${QPOASES_INCLUDE_DIR})
    set(QPOASES_LIBRARIES ${QPOASES_LIBRARY})
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(QPOASES_LIBRARIES "${QPOASES_LIBRARIES};m;pthread")
    endif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
endif(QPOASES_FOUND)

mark_as_advanced(QPOASES_LIBRARIES QPOASES_INCLUDE_DIR)
