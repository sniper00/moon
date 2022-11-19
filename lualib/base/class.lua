
local strfmt     = string.format

local rawget = rawget
local type = type
local assert = assert
local tostring = tostring
local getmetatable = getmetatable
local setmetatable = setmetatable
local error = error

local _iskindofinternal
_iskindofinternal = function(mt, classname)
    if not mt then return false end

    local index = rawget(mt, "__index")
    if not index then return false end

    local cname = rawget(index, "__cname")
    if cname == classname then return true end

    return _iskindofinternal(getmetatable(index), classname)
end

_G["iskindof"] = function(target, classname)
    local targetType = type(target)
    if targetType ~= "table" then
        return false
    end
    return _iskindofinternal(getmetatable(target), classname)
end

_G["class"] = function(classname, super)
    assert(type(classname) == "string", strfmt("class() - invalid class name \"%s\"", tostring(classname)))
    local superType = type(super)
    local cls
    if not super or superType == "table" then
        -- inherited from Lua Object
        if super then
            cls = {}
            setmetatable(cls, {__index = super})
            cls.super = super
        else
            cls = {ctor = function(...) end,__gc = true}
        end

        cls.__cname = classname
        cls.__index = cls

        function cls.new(...)
            local instance = setmetatable({}, cls)
            instance.class = cls
            instance:ctor(...)
            return instance
        end
    else
        error(strfmt("class() - create class \"%s\" with invalid super type",classname), 0)
    end
    return cls
end

