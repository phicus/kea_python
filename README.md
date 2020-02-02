# Kea Python
Develop Kea hooks in Python.

This hook embeds a Python3 interpreter in Kea and provides a thin wrapper oround a subset
of the Kea classes to allow you to develop hooks in Python.  Where possible the classes in
the kea Python module implement the same API as the Kea class so that there are no surprises
when using the module.  You should be able to follow the Kea developer guide at
https://jenkins.isc.org/job/Kea_doc/doxygen/.  After you have build the kea module you can
get a list of the contents using:
```
pydoc3 kea
```

Kea documentation is found at https://gitlab.isc.org/isc-projects/kea/wikis/home and can be
downloaded from https://www.isc.org/download/.

This project uses docker images so that the DHCP server and clients can be run on your
development workstation in a virtual network.  Installation of Docker Engine - Community as
is described at https://docs.docker.com/install/linux/docker-ce/ubuntu/ (for Ubuntu).

## Getting started
Download a Kea .tar.gz from https://www.isc.org/download/ and place it in your working
directory.

Most of the commands you will need to run are in a `Makefile`.  You can view the targets
by simply running `make`:
```
djc@laptop:~/play/kea_python$ make
run on host
  build-kea-dev   - build kea-dev:1.6.1 image
  build-kea       - build kea:1.6.1 image
  build-dhtest    - build dhtest image
  run-kea-dev     - run kea-dev:1.6.1 shell
  run-kea         - run kea:1.6.1 shell
  run-dhtest      - run dhtest shell
run inside kea-dev shell
  build-hook      - build and install libkea_python
  build-module    - build and install kea extension module
  test-module     - run unit tests for kea extension module
```

By default the project works with kea 1.6.1.  You can override that by specifying the version
in the environment:
```
djc@laptop:~/play/kea_python$ VER=1.7.3 make
run on host
  build-kea-dev   - build kea-dev:1.7.3 image
  build-kea       - build kea:1.7.3 image
  build-dhtest    - build dhtest image
  run-kea-dev     - run kea-dev:1.7.3 shell
  run-kea         - run kea:1.7.3 shell
  run-dhtest      - run dhtest shell
run inside kea-dev shell
  build-hook      - build and install libkea_python
  build-module    - build and install kea extension module
  test-module     - run unit tests for kea extension module
```

The first thing you need to do is build the `kea-dev` image.  This takes quite a while, but you
will only need to do it once.
```
djc@laptop:~/play/kea_python$ make build-kea-dev
```
The resulting image is close to 4G as it contains the Kea source, the installed Kea server and
a complete development environment.

This image intended to be used for C/C++ development on the `kea_python` hook.  If you are only
interested in working in Python then as soon as the build is complete you immediately build the
`kea` image.
```
djc@laptop:~/play/kea_python$ make build-kea
```
The `kea` image uses `kea-dev` to compile the `kea_python` hook and then discards all of the
development related files.  The saving is huge:
```
djc@laptop:~/play/kea_python$ docker images
REPOSITORY          TAG                 IMAGE ID            CREATED             SIZE
kea                 1.6.1               292b89ff6bc8        5 minutes ago       346MB
kea-dev             1.6.1               bdd11aaa16eb        8 minutes ago       3.55GB
debian              stretch-slim        2b343cb3b772        5 weeks ago         55.3MB
```

## Running examples
Most of the examples rely on `dhtest` to perform DHCP transactions.  You can build an image
containing `dhtest` using `make`:
```
djc@laptop:~/play/kea_python$ make build-dhtest 
```

Use `make run-kea` to run the `kea` image.  This creates a docker network and runs a command
shell.  Your working directory is mounted as `/workdir` inside the container.

For example, to run the facebook-trick example:
```
djc@laptop:~/play/kea_python$ make run-kea
root@92ddf6f5e9be:/# cd /workdir
root@92ddf6f5e9be:/workdir# /usr/local/sbin/kea-dhcp4 -c examples/facebook-trick/kea.conf 
```

Then in another shell:
```
djc@laptop:~/play/kea_python$ make run-dhtest 
root@375f50ed4699:/# dhtest -i eth0
```

## Performing development on kea_python
The `kea-dev` image is used when making changes to the `kea_python.so` hook.  All you need
to do is the following:
```
djc@laptop:~/play/kea_python$ make run-kea-dev 
root@a742a1b8b485:/source# cd /workdir
root@a742a1b8b485:/workdir# make build-hook build-module
```
Once the build finishes your container is able to run python hooks in the same was as the `kea`
image.  Your working directory is mounted as `/workdir` in the container.

Any time you make a change to the `keamodule` code you should rebuild it and run the unit tests
like this:
```
root@a742a1b8b485:/workdir# make build-module test-module
```
