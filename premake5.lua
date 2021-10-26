-- premake5.lua
workspace "CHIP8"
    configurations { "Debug", "Release" }
    platforms {"X64"}

project "CHIP8"
    kind "ConsoleApp"
    language "C++"
    targetdir "build/%{cfg.buildcfg}"
    links {"dl", "GL", "SDL2"}
    files { "src/**.h", "src/**.cpp" }

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"