## 实验流程

### 前提
1. master可以免密码ssh到slave上的root用户
2. slave节点上安装好了运行server所需要的依赖
* master 和 slave 都可以通过 `bash make_env_build.sh` 来安装依赖，此脚本在Ubuntu 22.04上通过测试，可以先在一个节点上运行该脚本然后制作一个镜像，再以此镜像来申请服务器
3. master节点上安装并运行了redis
* 可以通过 `bash init_manager.sh` 来安装并运行一个后台redis服务器
4. slave和master节点上安装了 python3 pip3 以及 redis, psutil包，可以使用  `bash init_worker.sh` 来安装

### 基本运行
以一个`.json`结尾的config来作为一次实验的全局配置，例如[exp_configs/cloudlab_config.json](exp_configs/cloudlab_config.json)，注意master和salve节点都会按照这个配置来运行

在master节点上执行命令
```bash
python3 manager.py --config exp_configs/cloudlab_config.json
```
来运行实验
给脚本加上参数`--init_worker`会自动安装依赖**运行python脚本所需要的包**

master节点上的python命令开始执行后，它会做以下几件事情：
1. 将master本地的[build/basic_server](build/basic_server)和[exp_utils/worker.py]以及输入的配置文件一起分发到所有的slave节点上，路径在[exp_utils/const.py]中配置，默认为`/root/exp/`
2. 在所有的slave节点上启动以screen后台的方式启动`worker.py`，并且会把输出log到默认的一个文件夹中。成功启动后，slave节点向master发送ack，此时slave节点会开始等待master节点发送命令
3. 收集到所有slave发来的ack之后，向所有的worker发送实验开始的命令
4. slave节点接受到实验开始的命令之后，在本地启动一个后台进程，运行`basic_server`，并将`basic_server`的basic_server的stderr和stdout重定向到slave本地的文件中，此后向master节点发送ack，然后开始定期向master节点发送心跳，心跳包含CPU和内存的利用率
5. master节点收到所有slave节点的ack之后，开始打印所有节点的心跳信息，同时陷入一个死循环（这个死循环中可以实现细粒度的控制实验的进度，例如向mds发送指令等，但是目前的实现就是一个死循环）
6. 此时可以去启动client mds等进程，连接所有的server开始完成实验
7. 实验结束时，需要手动的`ctrl+C`，让master节点陷入`KeyboardInterruptError`，此时master节点会向所有的slave节点发送实验结束的命令，slave节点收到命令之后会停止`basic_server`的运行，然后向master节点发送ack，master节点收到所有slave节点的ack之后，会把所有slave生成的stderr、stdout以及运行的log复制到本地的[logs/exp_start_time](logs/exp_start_time)文件夹中，`exp_start_time`为实验开始的时间

### 配置

可能需要修改的配置文件有下面几个：

- [exp_utils/redis_config.json](exp_utils/redis_config.json): redis相关的配置文件，只需要将`redis_host`改成运行manager.py的master节点的ip或者host名即可
- [exp_config/some_config.json](exp_configs/some_config.json): 这里面需要修改的东西有：
    * 当前需要创建的实验的配置
    * master和slave节点的ip等，注意manager.py会从这个配置文件中获取需要控制的所有slave的ip地址
- [exp_utils/const.py](exp_utils/const.py)：包含了一些常量，例如slave上存放所有实验相关文件的路径，通常不需要修改这个文件