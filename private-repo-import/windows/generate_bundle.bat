SETLOCAL

set BUNDLE_BUILDER=%1..\..\..\..\windows\bundle-builder
Rem echo %BUNDLE_BUILDER%

set IMAGE_NAME=bundle-builder
Rem echo %IMAGE_NAME%

set OUTPUT_DIR=%1..\..\..\..\cubzh\deps\xptools\windows
Rem echo %OUTPUT_DIR%

set BUNDLE=%1..\..\..\..\cubzh\bundle
Rem echo %BUNDLE%
set MODULES_SRC=%1..\..\..\..\cubzh\lua\modules
set I18N_SRC=%1..\..\..\..\cubzh\i18n

Rem "Debug" or "Release"
set CONFIGURATION=%2

if "%CONFIGURATION%" == "Debug" (
    if exist //./pipe/docker_engine (
        Rem Build the docker image
        docker build -t %IMAGE_NAME% %BUNDLE_BUILDER%
        docker run --rm --mount type=bind,source=%BUNDLE%,target=/src/bundle,readonly --mount type=bind,source=%I18N_SRC%,target=/src/i18n,readonly --mount type=bind,source=%MODULES_SRC%,target=/src/modules,readonly --mount type=bind,source=%OUTPUT_DIR%,target=/output %IMAGE_NAME%
    ) else (
        echo "---> BUNDLE GENERATION SKIPPED <---"
    )
)

if "%CONFIGURATION%" == "Release" (
    Rem Build the docker image
    docker build -t %IMAGE_NAME% %BUNDLE_BUILDER%
    docker run --rm --mount type=bind,source=%BUNDLE%,target=/src/bundle,readonly --mount type=bind,source=%I18N_SRC%,target=/src/i18n,readonly --mount type=bind,source=%MODULES_SRC%,target=/src/modules,readonly --mount type=bind,source=%OUTPUT_DIR%,target=/output %IMAGE_NAME%
)
