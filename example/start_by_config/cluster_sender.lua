local moon = require("moon")
local cluster = require("cluster")

local counter = 0

local timerid = moon.repeated(1000,-1,function( _ )
    print("per sec ",counter)
    counter=0
end)

local args = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}
moon.async(function()
    while true do
        local ret ,err = cluster.call(8, 'cluster_receiver', "ACCUM", table.unpack(args))
        if not ret then
            print(err)
            moon.remove_timer(timerid)
            return
        end
        counter=counter+1
    end
end)


moon.async(function()
    moon.sleep(5000)
    for i=1,1000000 do
        cluster.send(8, 'cluster_receiver',"COUNTER", moon.now())
    end
    print("end")
end)







