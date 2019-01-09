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
yum install snappy
git clone https://github.com/goole/snappy.git
cd snappy; cp snappy-c.h /usr/include
cd /usr/lib64; ln -s libsnappy.so.1.1.4 libsnappy.so

```
> 说明：如果安装失败，可以尝试更新源，比如使用ali源。
> 安装后，需要下载源码添加头文件；需再创建一个软链接。

(2) **安装效果举例**：
```
cd /usr/lib64; ls -alh | grep snappy
显示信息：
   libsnappy.so -> libsnappy.so.1.1.4
   libsnappy.so.1 -> libsnappy.so.1.1.4
   libsnappy.so.1.1.4
cd /usr/include; ls | grep snappy
   snappy-c.h
```

> 说明：安装目录可能存在差异，根据实际路径建立软链接。

### 2.2 安装说明

上述安装方法仅供参考，用户可以自行查找其它安装方法。
确保生成SST文件和解析SST文件时，使用的压缩或解压方法兼容即可。

## 3. 总结

该代码库的使用方法，参见doc目录下的文档，或参考test目录下的实例。
