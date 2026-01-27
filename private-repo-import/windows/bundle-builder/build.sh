#!/bin/ash

cp -R /src/bundle /bundle
cp -R /src/i18n /bundle
cp -R /src/modules /bundle

# Remove ds store file
find /bundle -name ".DS_Store" -delete

# Remove shaders that are not for Windows.
# We keep dx9, dx11, spirv (Vulkan) and glsl (OpenGL).
rm -r /bundle/shaders/essl
rm -r /bundle/shaders/metal

cd /bundle
tar -czvf /bundle.tar.gz .

cd /
./bin_to_c ./bundle.tar.gz bundle_tar_gz > /output/bundle_tar_gz.hpp