
macro(ModuleSetCompileOptions)
  CMAKE_POLICY(SET CMP0022 NEW)

  IF(POLICY CMP0048)
    CMAKE_POLICY(SET CMP0048 NEW)
  ENDIF()

  SET (CMAKE_C_STANDARD 99)

  IF (WIN32)
    MESSAGE(STATUS "Now is windows")

    SET(DMOS_NAME "win")

    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /DEBUG /OPT:REF /OPT:NOICF /STACK:16777216")
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /DEBUG /OPT:REF /OPT:NOICF /STACK:16777216")
    SET(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS}")
    IF(MSVC)
      ADD_DEFINITIONS(/bigobj)
      ADD_DEFINITIONS(/DNOMINMAX /DWIN32_LEAN_AND_MEAN=1 /D_CRT_SECURE_NO_WARNINGS /D_SCL_SECURE_NO_WARNINGS)

      ADD_COMPILE_OPTIONS(/W3 /wd4005 /wd4068 /wd4244 /wd4267 /wd4800)
      IF (MSVC_VERSION GREATER_EQUAL 1900)
        INCLUDE(CheckCXXCompilerFlag)
        CHECK_CXX_COMPILER_FLAG("/std:c++latest" _cpp_latest_flag_supported)
        IF (_cpp_latest_flag_supported)
          ADD_COMPILE_OPTIONS("/std:c++latest")
        ENDIF()
      ENDIF()    
    ENDIF()
  ELSEIF (APPLE)
    MESSAGE(STATUS "Now is Apple systems")

    SET(DMOS_NAME "mac") 

    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread -fPIC" )
    SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Og -g -D_DEBUG")
    SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -g")
    SET(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} -O3 -g")
    SET(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -O3 -g")

    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread -fPIC" )
    SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Og -g -D_DEBUG")
    SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -g")
    SET(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -O3 -g")
    SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O3 -g")

    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}" )
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
    SET(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS}")
    SET_PROPERTY(GLOBAL PREFIX "")
  ELSEIF (UNIX)
    MESSAGE(STATUS "Now is UNIX-like OS")

    SET(DMOS_NAME "lin")

    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread -fPIC" )
    SET(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Og -g -D_DEBUG")
    SET(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3 -g")
    SET(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} -O3 -g")
    SET(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -O3 -g")

    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread -fPIC" )
    SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Og -g -D_DEBUG")
    SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -g")
    SET(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -O3 -g")
    SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -O3 -g")

    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
    SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
    SET(CMAKE_STATIC_LINKER_FLAGS "${CMAKE_STATIC_LINKER_FLAGS}")

    SET_PROPERTY(GLOBAL PROPERTY PREFIX "")

    FIND_PROGRAM(CCACHE_FOUND ccache)
    IF(CCACHE_FOUND)
      SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
      SET_PROPERTY(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
    ENDIF(CCACHE_FOUND)
  ENDIF ()
endmacro(ModuleSetCompileOptions)
