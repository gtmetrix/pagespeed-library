# PageSpeed Insights

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
Use `gclient` (included with depot_tools) to pull in the PageSpeed source and its dependencies:
```shell
mkdir pagespeed-library
cd pagespeed-library
gclient config https://github.com/gtmetrix/pagespeed-library.git --name=src --unmanaged
git clone https://github.com/gtmetrix/pagespeed-library.git src
gclient sync
```
Finally to compile PageSpeed:
```
cd src
make -j4 BUILDTYPE=Release
```

## Update
```
cd pagespeed-library/src
git pull
gclient sync --force
```

## Notes

* PageSpeed originally had support for building in Windows and Mac, but recent changes have only been tested in Linux
* The code still depends on an old version of Chromium code so it seems to only build on GCC 4.7
