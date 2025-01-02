# DOCA lib


## build the library

```
meson setup build
ninja -C build/ -v
```

## integrate with other code base

This project will be compiled as a static library to be linked with other codes.

The static binary will be `libDOCA_lib.a` under the project directory after compilation.

### make


```
-L/path/to/this/repo -lDOCA_lib -libverbs
```

To use this library with a meson project, simply add `subdir('<path_to_this_lib>')` and then use the `libDOCA_lib_dep` in the dependencies of the compilation target.


Also add the `-I/path/to/DOCA_lib/include` to the cflags

### meson

Compile this code base to get the static library, then 

```
incdir = include_directories('DOCA_lib/include')
doca_lib_dep = declare_dependency(
  include_directories: incdir,
  link_args: ['-L' + root_dir + '/DOCA_lib', '-lDOCA_lib', ]
  )
```

Add the incdir to the include_directories of your target, then add the `doca_lib_dep` to the dependencies of your target.



## determine RDMA specific settings

The `-d`, `-x` and `-i` setting specifies the RDMA device index, sgid index and ip port settings. These settings should be adjusted on a per node basis.

Please follow the following steps to determine these values.
![](./figures/gid_instruction.png)

1. determine a interface to use, note the interfaces on two nodes should in same IP sub network so they can talk to each other.

2. Choose the row with v2 instead of v1, which stands for RoCEv2 support.

3. determine the device index, which is number in yellow square labeled 3, and the full name of the device.

4. determine the ip port setting(`-i`), which is the number in the yellow sqare labeled 4.

5. determine the sgid index setting(`-x`), which is the number in the yellow sqare labeled 5.

For example, follow the setting in the picture, we should be using `python exp1.py --n_core 16 --n_qp 128 -x 3 -i 1 -d 2` on the server node and `python exp1.py --n_core 16 --n_qp 128 -x 3 -i 1 -d 2 --server_ip 10.10.1.1` on the client node.


## examples

