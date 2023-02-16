# v0.7 (2023-01-15)
- Static link mimalloc default
- Add lua json config options
- Set BUFFER_HEAD_RESERVED 16 bytes
- Add moon.raw_dispatch
- Add default param for bootstrap service
- JSON "null" values are represented as lightuserdata

# v0.6 (2022-09-06)
- break changes
```
moon.remove_service -> moon.kill
moon.send_prefab -> moon.raw_send
moon.set_loglevel moon.get_loglevel -> moon.loglevel
moon.set_env moon.get_env -> moon.env
moon.make_response -> make_session
moon.size -> delete
moon.error_count -> delete
zset.key -> zset.key_by_rank
```
- use redis's skiplist implement zset
- add game server proxy template
- use lua extraspace save lua_service*
- close the connection when accept no_descriptors
- add uuid lualib
- update lua 5.4.5
- add mimalloc submodule

# v0.5 (2022-07-14)

- break changes
- - socket: rename PTYPE_TEXT to PTYPE_SOCKET_TCP
- - socket: rename PTYPE_SOCKET to PTYPE_SOCKET_MOON
- - socket remove data type socket_error
- update yyjson v0.5.1
- update asio v1.22.1
- add more test case
- compat new version lua-language-server code intellisense

# v0.4 (2022-05-25)

- update to lua5.4.4
- fix some lua error handle issues
- code optimize

# v0.3 (2022-01-27)

- add lua navmesh lib
- replace json lib with yyjson, decode speed 2x of rapidjson
- add json.decode option:null_as_userdata
- bugfix: http client connection pool reset
- improve redisd service:print redis request(base64) when error
- improve moon.log avoid memory copy

# v0.2 (2021-12-20)
- add mongodb driver
- improve message dispatch

 ```lua
    ---old style
    local function docmd(sender,sessionid, CMD,...)

    end

    moon.dispatch('lua',function(msg,unpack)
        local sender, sz, len = moon.decode(msg, "SC")
        docmd(sender, unpack(sz, len))
    end)


    ---new style
    moon.dispatch('lua',function(sender,session, cmd, ...)
        -- body
        local f = command[cmd]
        if f then
            f(sender,...)
        else
            error(string.format("Unknown command %s", tostring(cmd)))
        end
    end)
 ```

# v0.1 (2021-11-17)
- remove socket.readline. use: socket.read
- bugfix: buffer's move assignment operator
- message object passed by right value in cpp code
- use isocalendar calculate datetime
- improve code