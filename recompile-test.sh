#! /bin/bash

cd ~/xv6-k210/testsuits-for-oskernel/
sudo rm -rf ./riscv-syscalls-testing/user/build/
sudo rm -rf ./riscv-syscalls-testing/user/riscv64
docker run -ti --rm -v ./riscv-syscalls-testing:/testing -w /testing/user --privileged=true docker.educg.net/cg/os-contest:2024p6 /bin/bash -c "sh build-oscomp.sh"
cd ~/xv6-k210/
cp -r ~/xv6-k210/testsuits-for-oskernel/riscv-syscalls-testing/user/build/riscv64 .