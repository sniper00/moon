local moon = require("moon")
local cluster = require("moon.cluster")

local counter = 0

moon.start(function()

    local args = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}
    moon.async(function()

        while true do
            local ret ,err = cluster.call('server_5','cluster_example_receiver',"ACCUM",table.unpack(args))
            if not ret then
                print(err)
                return
            end
            counter=counter+1
        end
    end)
end)

moon.repeated(1000,-1,function( _ )
    print("per sec ",counter)
    counter=0
end)



