local function string_components(components)
    if not components then
        return ""
    end
    local str = ""
    for _, v in pairs(components) do
        if #str > 0 then
            str = str .. ",\n"
        end

        if v then
            str = str .. tostring(v)
        end
    end
    return str
end

local function string_components_ex(components)
    if not components or #components == 0 then
        return ""
    end
    local str = ""
    for _, v in pairs(components) do
        if #str > 0 then
            str = str .. ","
        end

        if v then
            str = str .. tostring(v)
        end
    end
    return str
end


local M = {}

M.__index = M

M.__tostring = function(t)
    return string.format("<Matcher [all=({%s}) any=({%s}) none=({%s})]>",
        string_components(t._all),
        string_components(t._any),
        string_components(t._none)
)
end

function M:matches(entity)
    local all_cond = not self._all or entity:has(table.unpack(self._all))
    local any_cond = not self._any or entity:has_any(table.unpack(self._any))
    local none_cond = not self._none or not entity:has_any(table.unpack(self._none))

    --print(all_cond,any_cond,none_cond)
    return all_cond and any_cond and none_cond
end

local function components_eql( comps1, comps2 )
    for k,v in pairs(comps1) do
        if not comps2[k] or not comps2[k]._is(v) then
            return false
        end
    end
    return true
end

local function components_intersect( comps1, comps2 )
    for k,v in pairs(comps1) do
        if comps2[k] and comps2[k]._is(v) then
            return true
        end
    end
    return false
end

function M:mcp(comp)
    local all_cond = not self._all or components_eql(self._all,comp)
    local any_cond = not self._any or components_intersect(self._any,comp)
    local none_cond = not self._none or not components_intersect(self._none,comp)

    --print(all_cond,any_cond,none_cond)
    return all_cond and any_cond and none_cond
end

local matcher_cache = {}

return function (all_of_tb, any_of_tb, none_of_tb)
    local key = string_components_ex(all_of_tb)..string_components_ex(any_of_tb)..string_components_ex(none_of_tb)
    local tb = matcher_cache[key]
    if not tb then
        tb = {_all = all_of_tb,_any =any_of_tb, _none = none_of_tb }
        matcher_cache[key] = setmetatable(tb, M)
    end
    return tb
end
