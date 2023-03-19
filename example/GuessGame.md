# moon服务器框架示例-多人猜数字小游戏

## 项目结构

```
.
├── game
│   ├── service_center.lua
│   ├── service_room.lua
│   └── service_user.lua
├── main_game.lua 
```

## Actor模型架构设计

- 唯一服务 `service_center` 负责匹配房间
- 房间服务 `service_room` 负责房间逻辑
- 玩家服务 `service_user` 负责玩家消息处理

## 启动

安装redis并启动，在上层目录运行
```
moon.exe example\main_game.lua
```

打开两个终端运行
```
telnet 127.0.0.1 8889

# 输入
login aaa
ready
```

```
telnet 127.0.0.1 8889
# 输入
login bbb
ready
```
