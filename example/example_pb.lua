local moon = require "moon"
local pb = require "pb"
local protoc = require "protoc"

local function raw_example()
    -- load schema from text (just for demo, use protoc.new() in real world)
    assert(protoc:load [[
    syntax = "proto3";
    message Phone {
        string name        = 1;
        int64  phonenumber = 2;
    }
    message Person {
        string name     = 1;
        int32  age      = 2;
        string address  = 3;
        repeated Phone  contacts = 4;
    } ]])

    -- lua table data
    local data = {
    name = "ilse",
    age  = 18,
    contacts = {
        { name = "alice", phonenumber = 12312341234 },
        { name = "bob",   phonenumber = 45645674567 }
    }
    }

    -- encode lua table data into binary format in lua string and return
    local bytes = assert(pb.encode("Person", data))
    print(pb.tohex(bytes))

    -- and decode the binary data back into lua table
    local data2 = assert(pb.decode("Person", bytes))
    print_r(data2)
end

raw_example()

moon.quit()
