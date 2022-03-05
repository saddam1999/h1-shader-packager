# h1-shader-packager
Tool to pack and unpack Halo PC (Retail and Custom Edition) shader archives

# Requirements
 * `cmake 3.22.1` or higher (although you can probably get away with less);
 * a compiler with support for `C++20` - `gcc 11.2` suffices;
 * `OpenSSL` - specifically the `crypto` library.

# Build Instructions
Build this project like your standard out-of-source build

# Usage
```
USAGE
  h1sp {-h|--help}
  h1sp {-u|--unpack} {-pc|-ce} {-fx|-vsh} INPUT_FILE [PREFIX] 
    Unpack the shader archive INPUT_FILE by writing each member to files prefixed with PREFIX.
  h1sp {-p|--pack} {-pc|-ce} {-fx|-vsh} OUTPUT_FILE [PREFIX]
    Creates a shader archive OUTPUT_FILE by packing members located via PREFIX.
  
  OPTIONS
     -pc indicates that the shader archive is for the retail client.
     -ce indicates that the shader archive is for the Custom Edition client.
     -fx indicates that the shader archive is an effects archive.
        PREFIX defaults to "fx/".
     -vsh indicates that the shader archive is a vertex shaders archive.
        PREFIX defaults to "vsh/".
```

For instance, to unpack the retail Effect archive, copy `shaders/fx.bin` from
the Halo PC installation directory to the build folder.

Then on the command line, enter
```
mkdir fx
./h1sp.exe -u -pc -fx fx.bin
```
or the equivalent in your environment.

To repack into `myfx.bin`:
```
./h1dp.exe -p -pc -fx myfx.bin
```

Assuming no modifications were made to the unpacked files, the files 
`fx.bin` and `myfx.bin` should have identical contents.

# License
This software is licensed under the Boost Software License (BSL-1.0).