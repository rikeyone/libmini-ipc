# Libsystem

Libsystem is a base system library for Linux or other OS. It is aimed to make userspace system designing goes quick and easy, you can easily build your own system rootfs based on libsystem. One can create your applications using this library for open source projects or commercial projects. 

This library drivers your applications to run by messages. Every application will create its own message queue and will wait for the messages to come. Developers can implement their requirements when the right message comes. This library makes your work team to co-work easily, because everyone can implements, compiles and tests his own applications at the same time. Big applications which have many functions, variables and threads are complicated to maintain. Libsystem trys to avoid this case: "one project, one application". Once you use libsystem, it gives you a choice to make project as a series of applications communicated with echo other. Every application just needs to focus on one service or one function.

# Compile

There is an Makefile in the source code directory. Just using:

```
make
make test
```

will compile the library and the test samples.

If you want cross compile, modify the Makefile:

```
CC=gcc
STRIP=strip
```

set these environment variables to the right cross compile tools.


# Usage

The API interface can be seen in **include** directory. You should include the .h in it and compile your applications with -lsystem -lrt -lpthread and -L{LIBSYSTEMDIR}. You can see the sample code in samples directory.
