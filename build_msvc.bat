@echo off

if not defined DevEnvDir (
    if "%VSWHERE%"=="" set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
)
if not defined DevEnvDir (
    for /f "usebackq tokens=*" %%i in (`call "%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath`) do (
      set InstallDir=%%i
    )
)
if not defined DevEnvDir (
    if exist "%InstallDir%\VC\Auxiliary\Build\vcvars64.bat" (
        call "%InstallDir%\VC\Auxiliary\Build\vcvars64.bat"
    )
)

if not exist .\hellovulkan\bin mkdir .\hellovulkan\bin

cl.exe /nologo /EHsc /Fo.\hellovulkan\bin\ /Fe.\hellovulkan\bin\ ^
    /I %VULKAN_SDK%\Include ^
    .\hellovulkan\src\Main.cpp ^
    .\hellovulkan\src\VulkanSample.cpp ^
    .\hellovulkan\src\VulkanQuad.cpp ^
    .\hellovulkan\src\VulkanTexturedQuad.cpp ^
    .\hellovulkan\src\Window.cpp ^
    .\hellovulkan\src\ImageIO.cpp ^
    .\hellovulkan\src\Utility.cpp ^
    %VULKAN_SDK%\Lib\vulkan-1.lib
