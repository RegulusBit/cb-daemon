# Install script for directory: /home/reza/development/conf-bank/v3/cb-daemon

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/home/reza/development/conf-bank/v3/cb-daemon/cmake-build-debug/src/utilities/cmake_install.cmake")
  include("/home/reza/development/conf-bank/v3/cb-daemon/cmake-build-debug/src/cblib/cmake_install.cmake")
  include("/home/reza/development/conf-bank/v3/cb-daemon/cmake-build-debug/src/zmq/cmake_install.cmake")
  include("/home/reza/development/conf-bank/v3/cb-daemon/cmake-build-debug/zcash/cmake_install.cmake")
  include("/home/reza/development/conf-bank/v3/cb-daemon/cmake-build-debug/primitives/cmake_install.cmake")
  include("/home/reza/development/conf-bank/v3/cb-daemon/cmake-build-debug/script/cmake_install.cmake")
  include("/home/reza/development/conf-bank/v3/cb-daemon/cmake-build-debug/crypto/cmake_install.cmake")
  include("/home/reza/development/conf-bank/v3/cb-daemon/cmake-build-debug/consensus/cmake_install.cmake")
  include("/home/reza/development/conf-bank/v3/cb-daemon/cmake-build-debug/snark/cmake_install.cmake")
  include("/home/reza/development/conf-bank/v3/cb-daemon/cmake-build-debug/support/cmake_install.cmake")
  include("/home/reza/development/conf-bank/v3/cb-daemon/cmake-build-debug/db/cmake_install.cmake")
  include("/home/reza/development/conf-bank/v3/cb-daemon/cmake-build-debug/zmqpp/cmake_install.cmake")
  include("/home/reza/development/conf-bank/v3/cb-daemon/cmake-build-debug/src/hashidsxx/cmake_install.cmake")

endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "/home/reza/development/conf-bank/v3/cb-daemon/cmake-build-debug/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
