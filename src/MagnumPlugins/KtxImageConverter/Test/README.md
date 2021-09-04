Updating test files
===================

The file dfd-data.bin is created using a patch to `testbidirectionalmapping`
from [dfdutils](https://github.com/KhronosGroup/dfdutils):

```bash
git clone https://github.com/KhronosGroup/dfdutils.git
cd dfdutils
git checkout 659a739bf60bdcd730b2159d658fb22e805854c2
git apply path/to/KtxImageConverter/Test/testbidirectionalmapping.c.patch
make testbidirectionalmapping
# if the above doesn't work, run gcc directly:
# gcc testbidirectionalmapping.c interpretdfd.c createdfd.c -o testbidirectionalmapping -I. -g -W -Wall -std=c99 -pedantic
./testbidirectionalmapping
```
