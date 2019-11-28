local DATA_TYPE_BASE = 1
local DATA_TYPE_ARRAY = 2
local DATA_TYPE_OBJ_ARRAY = 3
local DATA_TYPE_MAP = 4

local OP_GET = 1
local OP_SET = 2

local op_record = {}

op_record.__index = op_record

function op_record.new()
    local o = {record = {}}
    return setmetatable(o, op_record)
end

function op_record:insert(...)
    table.insert(self.record, ...)
end

function op_record:clear()
    self.record = {}
end

local object = {}

object.__index = object

local array = {}

array.__index = array

local object_array = {}

object_array.__index = object_array

local object_map = {}

object_map.__index = object_map

------------------------------------------------------
function object.new(op)
    local o = {}
    o.__op = op
    return setmetatable(o, object)
end

function object:open_record()
    if not self.__op then
        self.__op = op_record.new()
    end
end

function object:record()
    return self.__op.record
end

function object:clear_record()
    return self.__op:clear()
end

function object:set(key, value)
    self[key.id] = value
    if self.__op then
        self.__op:insert({OP_SET, key, value})
    end
end

function object:get(key)
    local t = self[key.id]
    if not t then
        if key.type == DATA_TYPE_ARRAY then
            t = array.new(self.__op)
        elseif key.type == DATA_TYPE_OBJ_ARRAY then
            t = object_array.new(self.__op)
        end
        self[key.id] = t
    end
    if self.__op then
        self.__op:insert({OP_GET, key})
    end
    return t
end

------------------------------------------------------

------------------------------------------------------
function array.new(op)
    local o = {}
    o.__op = op
    return setmetatable(o, array)
end

function array:set(index, value)
    assert(index <= #self + 1)
    self[index] = value
    if self.__op then
        self.__op:insert({OP_SET, index, value})
    end
end

function array:get(index)
    assert(index <= #self + 1)
    return self[index]
end

function array:has(value)
    for _,v in pairs(self) do
        if value == v then
            return true
        end
    end
    return false
end

------------------------------------------------------
------------------------------------------------------

function object_array.new(op)
    local o = {}
    o.__op = op
    return setmetatable(o, object_array)
end

function object_array:get(index)
    assert(index <= #self + 1)
    local o = self[index]
    if not o then
        o = object.new(self.__op)
        self[index] = o
    end
    if self.__op then
        self.__op:insert({OP_GET, index})
    end
    return o
end

------------------------------------------------------
------------------------------------------------------

function object_map.new(op)
    local o = {}
    o.__op = op
    return setmetatable(o, object_map)
end

function object_map:get(key)
    local o = self[key]
    if not o then
        o = object.new(self.__op)
        self[key] = o
    end
    if self.__op then
        self.__op:insert({OP_GET, key})
    end
    return o
end

------------------------------------------------------

return {
    object = object,
    DATA_TYPE_BASE = DATA_TYPE_BASE,
    DATA_TYPE_ARRAY = DATA_TYPE_ARRAY,
    DATA_TYPE_OBJ_ARRAY = DATA_TYPE_OBJ_ARRAY,
    DATA_TYPE_MAP = DATA_TYPE_MAP,
    OP_GET = OP_GET,
    OP_SET = OP_SET,
}
