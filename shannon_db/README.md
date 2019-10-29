# `libshannon_db.so`动态库简介

* 创建日期：2019.01.09
* 修改版本：0.0.1
* 修改人  ：chao.chen
* 主要改动：创建文档，说明snappy库的安装方法

## 1. 概要

该动态库为swift-kv的应用层C接口。

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
> 只用到了snappy-c.h头文件。

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
