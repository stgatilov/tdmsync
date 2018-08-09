# tdmsync

This library allows to update local file to its remote version stored on web server.
It uses the algorithm of [rsync][1] to reduce download size, and is very similar to [zsync][2] (in fact, is a simplified version of it).
It was originally made as better system of differential updates for [The Dark Mod][3] game.

### Build

In order to build tdmsync, you should install:

1. [cmake][4] for build system
2. [conan][5] for getting libcurl

If you don't want to mess with libcurl, you can set `WITH_CURL=OFF` in CMake configuration.
Then you don't need to install conan, but you won't be able to update files over HTTP (i.e. you will be limited to local updates).

[1]:https://en.wikipedia.org/wiki/Rsync
[2]:http://zsync.moria.org.uk/
[3]:http://www.thedarkmod.com/
[4]:https://cmake.org/
[5]:https://conan.io/