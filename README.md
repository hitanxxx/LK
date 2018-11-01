# LK
# Introduce
lk is a very small event-driven network-framework, build by ANSI C. it only supports Linux like operate system now, and require kernel version high than 2.4.  beside this, it also has three useful module, they are:
* lk-perf, a multi-process, data visualization table web application performance test tool.
* lk-tunnel, it's a proxy when it works in the single mode, it's an SSL tunnel when it works in the c/s mode. it's easy to deploy, it's a high performance, lightly, useful tool.
* lk-web, a small web server, support basial static file service. support network interface service for other modules.
# Install
it's rely on OpenSSL libs, make sure environment have OpenSSL libs before install.
be prepared, run this command:
* configure
* make && make install </br>
after this, lk can be found in /usr/local/lk
> * /usr/local/lk/conf  configuration file path
> * /usr/local/lk/logs  log file, temp file path
> * /usr/local/lk/sbin  elf file path
> * /usr/local/lk/www   HTML resource path
# Command line parameters
* -stop </br>
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
# More
[lk's wiki] (https://github.com/hitanxxx/lk/wiki)
