require("functions")
require("Log")

local Component = class("Component")

function Component:ctor()
    self.components = { }
    self.name = "Unkonwn Component"
    self.owner = nil
    self.enableupdate = false

    Log.Trace("ctor Component")

end

function Component:GetName()
    return self.name
end

function Component:SetName(name)
    self.name = name
end

function Component:GetOwner()
    return self.owner
end

function Component:SetOwner(owner)
    self.owner = owner
end

function Component:AddComponent(name, cpt)
    assert(self.components[name] == nil, string.format("Component named [%s] already exist!", name))
    self.components[name] = cpt
end

function Component:GetComponent(name)
    assert(nil ~= name, "GetComponent name must not be nil")
    return self.components[name]
end

function Component:Start()
    for _, v in pairs(self.components) do
        v:Start()
    end
end

function Component:Update()
    for _, v in pairs(self.components) do
        if v:IsEnableUpdate() then
            v:Update()
        end
    end
end

function Component:Destory()
    for _, v in pairs(self.components) do
        v:Destory()
    end
end

function Component:SetEnableUpdate(b)
    self.enableupdate = b
end

function Component:IsEnableUpdate()
    return self.enableupdate
end

function Component:OnClientClose(player)
    for _, v in pairs(self.components) do
        v:OnClientClose(player)
    end
end

return Component