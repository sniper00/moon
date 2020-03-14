local moon = require("moon")
local httpclient = require("moon.http.client")

moon.start(function()
    moon.async(function()
        -- 注意只支持单个返回值， 多返回值请使用{} or table.pack
		local co1 = moon.async(function()
			return 1
		end)

		local co2 = moon.async(function()
			moon.sleep(5000)
			return 2
		end)

		local co3 = moon.async(function()
			moon.sleep(2000)
			return 3
		end)

		local co4 = moon.async(function()
			moon.sleep(1000)
			return 4
		end)

		local res =  moon.wait_all(co1,co2,co3,co4)
		print_r(res)
    end)

    moon.async(function()
		local co1 = moon.async(function()
			return httpclient.get("www.baidu.com")
		end)

		local co2 = moon.async(function()
			return httpclient.get("cn.bing.com")
		end)

		local res =  moon.wait_all(co1,co2)
		print_r(res)

		moon.abort()
	end)
end)