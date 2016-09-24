{
  "targets": [
    {
      "target_name": "vle_node",
      "sources": [ "wrapper.cpp" ],
      "include_dirs": [ "<!@(pkg-config --cflags-only-I vle-1.1 | sed s/-I//g)" ],
      "libraries": [ "<!@(pkg-config --libs vle-1.1)" ],
      "cflags!": [ '-fno-exceptions' ],
      "cflags": [ "-std=c++11" ],
      "cflags_cc!": [ "-fno-exceptions", "-fno-rtti" ]
    }
  ]
}