# 服务器使用config.json作为配置文件
可以配置多个服务器相关设置，和服务器启动时创建的服务。同时作为集群通信的配置文件。

## server配置

属性名 | 数据类型 | 默认值 | 说明 | 其他 
-- | :-: | :-:| :-: | -: 
sid | int|必须配置| 服务器id | 其他属性配置可以使用`#sid`替代具体值。不能重复。
name | string|必须配置| 服务器名 | 示例 如：sever_#sid
outer_host | string| 必须配置 | 外部ip | 其他属性配置可以使用`#outer_host`替代具体值
inner_host | string| 127.0.0.1 | 内部ip | 其他属性配置可以使用`#inner_host`替代具体值
thread | int| cpu核心数 | worker线程数 | 
startup | string|  | 启动脚本 | 可空，可以在启动脚本做一些全局初始化，如加载pbc协议文件
log | string|  | 日志文件路径 | 如：logpath/#sid_#date.log #date当前日期。 为空时将不再写日志文件，只在控制台输出。
loglevel | string| DEBUG | 日志等级 | 可选 DEBUG，INFO，WARN，ERROR
path | string|  | lua搜索路径 |

## sevice配置

属性名 | 数据类型 | 默认值 | 说明 | 其他 
-- | :-: | :-:| :-: | -: 
type |string| lua| 服务类型 | lua编写的服务类型都为lua,可以cpp编写自定义类型的服务
unique |bool| false| 服务类型 | 是否是唯一服务，唯一服务可以用moon.unique_service（name）获取服务id.唯一服务crash时，进程会直接退出。
shared |bool| true| 是否和其他服务共享worker线程 | 用于服务独享一个线程
threadid |int| 0| 服务的worker线程id | 用于服务线程绑定，0不绑定，范围1-thread
name |string|必须配置 | 服务name
network | json||用于配置网络相关  

## network

属性名 | 数据类型 | 默认值 | 说明 | 其他
-- | :-: | :-:| :-: | -: 
name |string| 必须配置| netwokr name,方便获取
timeout |int| 0| 连接read超时时间，单位秒。 0不检测超时
ip |string| 必须配置| 如 #inner_host
port |int| 必须配置|
type |string| listen| type为listen时会直接绑定地址，其他值无作用
protocol |int| 0| 0：2字节大端长度开头的协议。1：自定义协议。2：websocket(server only)

## 配置示例

```json
[
    {
        "sid": 1,
        "name": "server_#sid",
        "outer_host": "127.0.0.1",
        "inner_host": "127.0.0.1",
        "thread": 4,
        "startup": "main.lua",
        "log": "log/#sid_#date.log",
        "services": [
            {
                "type": "lua",
                "unique": true,
                "shared": false,
                "threadid": 0,
                "name": "clusterd",
                "file": "service/clusterd.lua",
                "network": {
                    "name": "network",
                    "type": "listen",
                    "ip": "#inner_host",
                    "port": "10001"
                }
            },
            {
                "name": "network_example",
                "file": "network_example.lua",
                "network": {
                    "name": "network",
                    "type": "listen",
                    "ip": "#inner_host",
                    "port": "12345",
                    "protocol":2
                }
            }
        ]
    },
    {
        "sid": 2,
        "name": "server_#sid",
        "log": "log/#sid_#date.log",
        "services": [
            {
                "name": "redis_example",
                "file": "redis_example.lua"
            }
        ]
    }
]
```
