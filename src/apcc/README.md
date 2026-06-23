How to use the lib in your project:

1) Clone the vcpkg repo and install the required packages
   - git clone https://github.com/microsoft/vcpkg.git
   - cd vcpkg && bootstrap-vcpkg.bat
   - .\vcpkg.exe integrate install
   - .\vcpkg install jansson
   - .\vcpkg install libwebsockets
   - .\vcpkg install glib

2) Compile libwebsockets with extensions:
   -  edit .\vcpkg\ports\libwebsockets\portfile.cmake
   -  to cmake_configure options add the flag -DLWS_WITHOUT_EXTENSIONS=OFF

4) Create a visual studio project and add APCc.c + APCc.h

5) Add additional include directory ``$(_ZVcpkgCurrentInstalledDir)/include/glib-2.0;$(_ZVcpkgCurrentInstalledDir)/lib/glib-2.0/include;``
