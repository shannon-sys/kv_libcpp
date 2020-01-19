# `libshannondb_cxx.so`动态库简介

* 创建日期：2019.01.09
* 修改版本：0.0.1
* 修改人  ：chao.chen
* 主要改动：创建文档，说明snappy库的安装方法

## 1. 概要

该库为swift-kv的应用层`C++`接口。

## 2. 依赖

该代码库在编译和运行时，需预先安装的动态库包括：
+ libsnappy.so

> 说明： Rocksdb生成SST时，可能对内容压缩。在解压缩时，需要对应的压缩库。
> 实现备份功能，即解析SST文件时，依赖了对应的压缩库。

### 2.1 snappy库安装方法

#### 2.1.1 Ubuntu16.04下安装

(1) **安装方法**：

```
sudo apt-get install libsnappy-dev

```
> 说明：如果安装失败，可以尝试更新源，比如ali源。

(2) **安装效果举例**：
```
cd /usr/lib/x86_64-linux-gnu; ls -alh | grep snappy
显示信息：
   libsnappy.a
   libsnappy.so -> libsnappy.so.1.3.0
   libsnappy.so.1 -> libsnappy.so.1.3.0
   libsnappy.so.1.3.0
cd /usr/include; ls | grep snappy
   snappy-c.h
   snappy.h
   snappy-sinksource.h
   snappy-stubs-public.h
```
> 说明：安装目录可能存在差异，可以查一查实际的安装路径。
> 只用到了**snappy-c.h**头文件，代码实现只调用C接口函数。

#### 2.1.2 Centos下安装方法

(1) **安装方法**：

```
sudo yum install snappy snappy-devel

```
> 说明：如果安装失败，可以尝试更新源，比如使用ali源。
> 默认安装的版本为1.1.4。

(2) **安装效果举例**：
```
cd /usr/lib64 && ls -alh | grep snappy
显示信息：
   libsnappy.so -> libsnappy.so.1.1.4
   libsnappy.so.1 -> libsnappy.so.1.1.4
   libsnappy.so.1.1.4
cd /usr/include && ls | grep snappy
显示信息：
   snappy-c.h
```

(3) **源码安装方法（可选）**

比如源码安装1.1.7版本（需要CMake 3.1以上）：

git clone https://github.com/google/snappy.git
cd snappy
git checkout 1.1.7

默认编译生成的是静态库。若要生成动态链接库，需要修改文件CMakeLists.txt，
找到 add_library(snappy "") 这一行，改为 add_library(snappy SHARED "")

mkdir build && cd build
cmake ../ && make
sudo make install

对应的库将安装到目录 /usr/local/lib64/ 下面。应用程序使用时需要将该目录加入链接路径。
对应的头文件在目录 /usr/local/include/ 下面。

### 2.2 安装说明

上述安装方法仅供参考，用户可以自行查找其它安装方法。
确保生成SST文件和解析SST文件时，使用的压缩或解压方法兼容即可。

## 3. 总结

该代码库的使用方法，参见doc目录下的文档，或参考test目录下的实例。

#迁移工具使用说明

##支持的格式
1. zlib
2. zstd
3. lz4
4.  lz4hcc
5.  bzip2
6.  snappy
##编译
会需要提前安装对应的库文件
可选择命令，具体看操作系统

```
 yum -y install gcc gcc-c++
 yum -y install snappy snappy-devel zlib zlib-devel bzip2 bzip2-devel lz4 lz4-devel zstd
```
可以选择单独编译某种压缩格式，如果你不清楚使用哪种压缩格式也可以选择全部打开，也可以同时使用两种或者多种压缩格式。
```
ALL_COMPRES=ture   				打开所有的压缩格式
ZLIB=ture						使用zlib
ZSTD=ture						使用zstd
BZIP2=ture						使用bzlib2
LZ4=ture						使用lz4和lz4hcc
```
在kv_libc++下编译，使用 make 来编译，编译的同时加上对应的参数
**比如**
```
make ZLIB=true LZ4=true
```
然后在kv_libc++下编译migrate工具
```
// 使用和上面编译kv_libc++时相同的参数，否则可能运行不正常，和上面一致
make migrate ZLIB=true LZ4=true
```
这样就可以使用

此工具会要求输入要导入的db的名字（可以和导入之前不同），ColumnFamily的名字（必须和导入的源的数量名字完全一致，如果只需要默认创建输入default即可），需要导入的sst文件的地址。

```
please cin dbname
db
please cin ColumnFamily name split with a blank, exit with "default"(mean use default db cf)
default
please cin sst file split with a blank
./test/a.sst
```

之后就会自动导入sst文件到卡上
###注意
1. 该导入操作之前需要安装驱动
2. 在导入的时候会删除之前的同名db然后重新创建
3. 一次只能导入一个db，如果有多个db请多次导入
4. 如果sst文件较多，可以使用shell实现导入操作

实例
```
    echo -e $filename"\ndefault" > $filename".config"
    file_path=$path$filename
    cd $file_path
    ssts=$(ls *.sst)
    fil=""
    for sst in $ssts
    do
        str=$file_path"/"$sst
        fil+=" "$str
    done
    echo -e $fil >> "../../"$filename".config"
    cd ../../
    ./migrate < $filename".config"
```