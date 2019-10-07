# EASY
Efficient Arbiter SYnthesis (EASY) -  a plugin to the LegUp High Level Synthesis tool that simplifies the memory arbitration with the help of formal verifications.



## Building

### Requirements

[LegUp HLS tool](http://legup.eecg.utoronto.ca)

[Microsoft Boogie](https://github.com/boogie-org/boogie)

### LegUp Building

Put the LLVM pass folder to the LegUp LLVM directory:
```
cp -r easy $LEGUP/llvm/lib/Transforms/
if ! grep -q "add_subdirectory(easy)" $LEGUP/llvm/lib/Transforms/CMakeLists.txt; then
  echo "add_subdirectory(EASY)" >> $LEGUP/llvm/lib/Transforms/CMakeLists.txt
fi
```

Build LegUp
```
cd $LEGUP
make
```

## Test Examples

You can use `quick_start.sh` to play with existing examples:

```
# bash quick_start.sh $NUM_PARTITION $BENCHMARK_NAME
bash quick_start.sh 08 histogram

```

## Publication

If you use EASY in your research, please cite [our FPGA2019 paper](http://cas.ee.ic.ac.uk/people/gac1/pubs/JianyiFPGA19.pdf)

```
@inproceedings{cheng-easy-fpga2019,
 author = {Cheng, Jianyi and Fleming, Shane T. and Chen, Yu Ting and Anderson, Jason H. and Constantinides, George A.},
 title = {EASY: Efficient Arbiter SYnthesis from Multi-threaded Code},
 booktitle = {Proceedings of the 2019 ACM/SIGDA International Symposium on Field-Programmable Gate Arrays},
 year = {2019},
 address = {Seaside, CA, USA},
 pages = {142--151},
 numpages = {10},
 doi = {10.1145/3289602.3293899},
 publisher = {ACM},
}
```

## Contact

Any questions or queries feel free to contact me on: jianyi.cheng17@imperial.ac.uk

