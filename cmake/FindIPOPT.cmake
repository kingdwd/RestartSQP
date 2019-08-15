set(IPOPT_ROOT_DIR "$ENV{IPOPT_ROOT_DIR}" CACHE PATH "IPOPT root directory.")
message("Looking for Ipopt in ${IPOPT_ROOT_DIR}")


find_path(IPOPT_INCLUDE_DIR
	NAMES IpNLP.hpp 
	HINTS ${PROJECT_SOURCE_DIR}/third_party/CoinIpopt/build/include/coin
	HINTS /usr/local/include/coin
	HINTS ${IPOPT_ROOT_DIR}/include/coin
	HINTS ${IPOPT_ROOT_DIR}/include
)

if(APPLE)
find_library(IPOPT_LIBRARY 
	libipopt.dylib
	HINTS /usr/local/lib
	HINTS ${PROJECT_SOURCE_DIR}/third_party/CoinIpopt/build/lib
	HINTS ${IPOPT_ROOT_DIR}/lib
)
find_library(IPOPT_LIBRARY2 
	libcoinasl.dylib
	HINTS /usr/local/lib
	HINTS ${PROJECT_SOURCE_DIR}/third_party/CoinIpopt/build/lib
	HINTS ${IPOPT_ROOT_DIR}/lib
)
find_library(IPOPT_LIBRARY3 
	libipoptamplinterface.dylib
	HINTS /usr/local/lib
	HINTS ${PROJECT_SOURCE_DIR}/third_party/CoinIpopt/build/lib
	HINTS ${IPOPT_ROOT_DIR}/lib
)

elseif(UNIX)
find_library(IPOPT_LIBRARY 
	libipopt.so
	HINTS /usr/local/lib
	HINTS ${PROJECT_SOURCE_DIR}/third_party/CoinIpopt/build/lib
	HINTS ${IPOPT_ROOT_DIR}/lib
)
find_library(IPOPT_LIBRARY2 
	libcoinasl.so
	HINTS /usr/local/lib
	HINTS ${PROJECT_SOURCE_DIR}/third_party/CoinIpopt/build/lib
	HINTS ${IPOPT_ROOT_DIR}/lib
)
find_library(IPOPT_LIBRARY3 
	libipoptamplinterface.so
	HINTS /usr/local/lib
	HINTS ${PROJECT_SOURCE_DIR}/third_party/CoinIpopt/build/lib
	HINTS ${IPOPT_ROOT_DIR}/lib
)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(IPOPT DEFAULT_MSG IPOPT_LIBRARY IPOPT_INCLUDE_DIR)

if(IPOPT_FOUND)
	message("—- Found Ipopt under ${IPOPT_INCLUDE_DIR}")
    set(IPOPT_INCLUDE_DIRS ${IPOPT_INCLUDE_DIR})
    set(IPOPT_LIBRARIES ${IPOPT_LIBRARY} ${IPOPT_LIBRARY2} ${IPOPT_LIBRARY3})
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(IPOPT_LIBRARIES "${IPOPT_LIBRARIES};m;pthread")
    endif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
endif(IPOPT_FOUND)

mark_as_advanced(IPOPT_LIBRARIES IPOPT_INCLUDE_DIR)
