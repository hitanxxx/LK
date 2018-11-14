# LK

# Introduce
LK 的本意是构建一个web框架，不过web框架多如过江之鲫。更何况其他框架都用脚本了，LK还得写代码。所以还是说说它能干吗。它现在有3个模块：
## LK-PERF
lk-perf 多进程事件驱动，数据可视化的，web应用性能测试软件。web前端用echarts js实现数据可视化，通过UI可以方便的设置多web应用测试用例，直观的进行对比。
## LK-TUNNEL
lk-tunnel 无状态，无缓存的http proxy，SSL tunnel。跑在树莓派module b（1GB ddr2）上运行。看youtube的720p的视频，cpu使用率以及内存占用率为2%。一个有意思的用法是搭配lk-perf来使用，佐以大量的假请求。混淆真实请求。
## LK-WEB
lk-web 一个web服务器，有一个简单的路由支持类似rest api（严格来说不是restful）服务。以及静态文件服务。

# Install
lk的功能模块需要OpenSSL库。解决依赖后。在文件目录运行：
* configure
* make && make install </br>
即可完成安装。
在 centos7 与 debian系raspbian上都成功编译。
安装完成后，在/usr/local/lk目录可看到安装完成后的文件。
运行/usr/local/lk/sbin目录下的elf文件即可使用，但是使用之前可能需要了解配置。
> * /usr/local/lk/conf - configuration file path
> * /usr/local/lk/logs - log file, temp file path
> * /usr/local/lk/sbin - elf file path
> * /usr/local/lk/www  - HTML resource path

# Command line parameters
目前支持的命令行参数只有一个：
* -stop </br>
作用是停止后台所有lk进程。</br>
stop all process when works in the backend

# configuraction
```json
{
	"daemon":true,
	"worker_process":2,

	"reuse_port":false,
	"accept_mutex":false,

	"log_error":true,
	"log_debug":false,

	"sslcrt":"/usr/local/lk/www/certificate/server.crt",
	"sslkey":"/usr/local/lk/www/certificate/server.key",

	"http":{
		"access_log":true,
		"keepalive":false,
		"http_listen":[80],
		"https_listen":[443],
		"home":"/usr/local/lk/www",
		"index":"index.html"
	},
	"tunnel":{
		"mode":"single"
	},
	"perf":{
		"switch":false
	}

}
```
一个典型的配置文件如上，结构很简单，功能一目了然。
the configuration file can be divided into three part:
* global block
> * daemon - daemon mode switch
> * worker_process - number of worker_process
> * reuse_port - socket's features
> * accept_mutex - manual mutex lock
> * log_error - error log of all
> * log_debug - debug log of all
> * sslcrt - ssl service needs
> * sslkey - ssl service needs
* http block
> * access_log - http module access log
> * keepalive - http/1.1 keepalive
> * http listen - http listen ports
> * https listen - https listen ports
> * home - static file path
> * index - static file default suffix
* tunnel blcok
> * mode - single/client/server
> * serverip - client mode need specify 'serverip'
* perf block
> * switch - performance test mode switch
# Other
* 通过 /perform.html页面可以进入lk-perf的web页面。
* tunnel的mode为single时，为proxy模式。默认7325端口。
* 为server时，为ssl tunnel的server模式。
* 为client时，为ssl tunnel的client模式，需要配置 serverip字段，制定一个server的ip地址。以与server组成SSL tunnel。默认7325端口。
# Benchmark
## lk-perf VS apachebench
宿主 cpu为 3.8Ghz, 内存ddr4-2400，网络为千兆以太</br>
均为http，connection close短连接。
### 单核心
> * lk-perf 虚拟机 1核心，1gb内存
> * ab 虚拟机 1核心，1gb内存
> * 被测机- nginx 1.14，虚拟机 1核心，1gb内存，1进程。

|    object  |   进程 | 并发设置 | 时间     |  test1    |  test2   |   test3     |  cpu      |
|------------|--------|----------|-----------|-----------|---------|-------------|-----------|
|    lk perf |   1    | 30       |   10s     |70679      |70637    |70580        |   99      |
|    ab      |   /    |  1000    |   10s     |72251      |71711    |72433        |   99      |

### 双核心
> * lk perf 虚拟机 2核心，1gb内存
> * ab虚拟机 2核心，1gb内存
> * 被测试的nginx-1.14，虚拟机 2核心，1gb内存，2进程。

|    object   |   进程  | 并发设置 | 时间      |  test1   |test2     |   test3  |  cpu     |
|-------------|---------|----------|-----------|----------|----------|----------|----------|
|    lk perf  |   2     | 30       |   10s     |194177   |195576     |195954    |  97-96   |
|    ab       |   /     |  3000    |   10s     |181247   |161376     |170303    |   100    |
> *  ab 的test2，test3出现了failed的情况。数量在400左右。
> *  双核心数据和单核心的数据比起来，并不是与单核心的数据x2差不多，而是多了不少。这有点不合逻辑，我不知道为什么。也许是因为虚拟机的关系双核心的情况下占用了比预期更多宿主资源导致的。不过这里想说明的是，对比保持了环境的一致性。数据一定程度上总能体现不同对象在同样环境下的差异。
## lk-web VS nginx1.14
### 单核心
> * lk-web 虚拟机 1核心，1gb内存
> * nginx 1.14 虚拟机 1核心 1gb内存
> * 测试机 lk-perf 虚拟机 1核心，1gb内存
lk-perf 采用 30路并发，10s时间

|     object  |    进程    | 时间      |  test1    |  test2  |   test3    |  cpu     |
| ------------|------------|-----------|-----------|---------|-------------|----------|
|    lk web   |   1        |   10s     |77373      |77037    |77475        |   94     |
|    nginx    |   1        |   10s     |70718      |71082    |70215        |   90     |

### 双核心
> * lk-web 虚拟机 2核心，1gb内存
> * nginx 1.14 虚拟机 2核心 1gb内存
> * 测试机 lk-perf 虚拟机 2核心，1gb内存
lk-perf 采用 30路并发，10s时间

|     object |    进程     | 时间      |  test1    |  test2  |   test3     |  cpu     |
|------------|-------------|-----------|-----------|---------|-------------|-----------|
|    lk web  |   2         |   10s     |194854     |193158   |192333       |   92-92   |
|    nginx   |   2         |   10s     |186471     |185138   |184547       |   95-95   |

> * 这里虽然用nginx作为lk-web服务器性能的比较对象。但lk web的作用更多的是为其他模块如 lk perf提供服务。它的功能十分简单，流程也很精简，而nginx功能丰富。 与nginx的性能对比没有太多实际意义。但是能够一定程度上说明lk的性能。
