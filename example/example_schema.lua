local moon = require("moon")
local schema = require("schema")
local json = require("json")

local proto_define = [[
    {
        "array_int64": {
            "data": {
                "container": "array",
                "value_type": "int64",
                "value_index": "1"
            }
        },
        "UserData": {
            "uid": {
                "value_type": "int64",
                "value_index": "2",
                "comment": "玩家uid"
            },
            "name": {
                "value_type": "string",
                "value_index": "3",
                "comment": "玩家名字"
            },
            "level": {
                "value_type": "int32",
                "value_index": "4",
                "comment": "玩家等级"
            },
            "itemlist": {
                "container": "object",
                "key_type": "int32",
                "value_type": "ItemData",
                "value_index": "11",
                "comment": "道具列表"
            },
            "taskrewardgetlist": {
                "container": "object",
                "key_type": "int32",
                "value_type": "array_int64",
                "value_index": "78",
                "comment": "已领取任务奖励id缓存"
            }
        },
        "ItemData": {
            "id": {
                "value_type": "int32",
                "value_index": "1",
                "comment": "道具id"
            },
            "count": {
                "value_type": "int64",
                "value_index": "2",
                "comment": "道具数量"
            },
            "reward": {
                "value_type": "Reward",
                "value_index": "3",
                "comment": "test"
            }
        },
        "Reward": {
            "type": {
                "value_type": "int32",
                "value_index": "1",
                "comment": "怪物类型"
            },
            "rewardtimes": {
                "value_type": "int32",
                "value_index": "2",
                "comment": "已奖励次数"
            },
            "buyrewardtimes": {
                "value_type": "int32",
                "value_index": "3",
                "comment": "钻石购买已奖励次数"
            }
        }
    }
]]

-- load once then shared by other services
schema.load(json.decode(proto_define))

print(pcall(schema.validate, "UserData", {name = 123}))
print(pcall(schema.validate, "UserData", {level = 1.234}))
print(pcall(schema.validate, "UserData", {taskrewardgetlist = {[1] = {1,2,3,false}}}))
print(pcall(schema.validate, "UserData", {itemlist = {a ="123"}}))
print(pcall(schema.validate, "UserData", {itemlist = {1,2,3}}))
print(pcall(schema.validate, "UserData", {itemlist = {[1] = {a= 123}}}))
print(pcall(schema.validate, "UserData", {itemlist = {[1] = {id= 123, count = 123, reward = {type = 123, buyrewardtimes= false, rewardtimes = 100}}}}))

--[[
2023-08-15 20:40:20.787 | :01000001 | INFO  | false     'UserData.name' string expected, got number, value '123'. trace: UserData.name  (example_proto_verify.lua:84)
2023-08-15 20:40:20.787 | :01000001 | INFO  | false     'UserData.level' int32 expected, got number, value '1.234'. trace: UserData.level       (example_proto_verify.lua:85)
2023-08-15 20:40:20.787 | :01000001 | INFO  | false     'array_int64.data.4' int64 expected, got boolean. trace: UserData.taskrewardgetlist.1.data.4    (example_proto_verify.lua:86)
2023-08-15 20:40:20.787 | :01000001 | INFO  | false     'UserData.itemlist.$key' int32 expected, got string. trace: UserData.itemlist.a (example_proto_verify.lua:87)
2023-08-15 20:40:20.788 | :01000001 | INFO  | false     'ItemData' table expected, got number. trace: UserData.itemlist.1       (example_proto_verify.lua:88)
2023-08-15 20:40:20.788 | :01000001 | INFO  | false     Attemp to index undefined field: 'ItemData.a'. trace: UserData.itemlist.1.a     (example_proto_verify.lua:89)
2023-08-15 20:40:20.788 | :01000001 | INFO  | false     'Reward.buyrewardtimes' int32 expected, got boolean, value 'false'. trace: UserData.itemlist.1.reward.buyrewardtimes    (example_proto_verify.lua:90) 
]]

moon.exit(0)