# Page Speed Insights

This is a fork of https://github.com/pagespeed/page-speed with the following changes:

* Updated the DEPS to use working repositories.
* Removed deprecated browser addons/extensions.
* Moved required dependencies into the third_party folder.

## Check out and compile

You will need [depot_tools](https://www.chromium.org/developers/how-tos/install-depot-tools):
```shell
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
# Add depot_tools to $PATH
export PATH="$PATH:`pwd`/depot_tools"
```
Use `gclient` (included with depot_tools) to pull in the Page Speed source and its dependencies:
```shell
mkdir page-speed-library
cd page-speed-library
gclient config https://github.com/gtmetrix/page-speed-library.git --name=src --unmanaged
git clone https://github.com/gtmetrix/page-speed-library.git src
gclient sync
```
Finally to compile Page Speed:
```
cd src
make -j4 BUILDTYPE=Release
```

## Update
```
cd page-speed-library/src
git pull
gclient sync --force
```

## Notes

* Page Speed originally had support for building in Windows and Mac, but recent changes have only been tested in Linux
