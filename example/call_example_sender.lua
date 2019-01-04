local moon = require("moon")

moon.start(
    function()
        moon.async(
            function()
                local receiverid = moon.co_new_service(
                    "lua",
                    {
                        name = "call_example_receiver",
                        file = "call_example_receiver.lua"
                    }
                )

                local receiverid2 = moon.co_new_service(
                    "lua",
                    {
                        name = "remove_service_example",
                        file = "call_example_receiver.lua"
                    }
                )


                print(moon.co_call("lua", receiverid, "SUB", 1000, 2000))
                moon.co_wait(1000)
                print(moon.co_call("lua", receiverid, "ACCUM", 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16))

                moon.co_wait(1000)
                print("*************This call will got error message:")
                print(moon.co_call("lua", receiverid, "ADD", 100, 99))

                moon.co_wait(1000)
                print("*************This call will success:")
                print(moon.co_call("lua", receiverid, "SUB", 100, 99))

                moon.co_wait(1000)
                print("*************Let receiver exit:")
                print(moon.co_call("lua", receiverid, "EXIT"))

                moon.co_wait(1000)
                print("*************This call will got service exited:")
                print(moon.co_call("lua", receiverid, "SUB", 1000, 2000))

                moon.co_wait(1000)
                print("*************Remove other service:")
                print(moon.co_remove_service(receiverid2))

                moon.co_wait(1000)
                print("*************Call nonexistent service:")
                print(moon.co_call("lua", receiverid2+1, "SUB", 1000, 2000))
            end
        )
    end
)
