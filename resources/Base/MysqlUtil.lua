
local MsgID 		= require("MsgID")
local BinaryReader 	= require("BinaryReader")
local Stream		= require("Stream")

local QUERY = 1
local EXECUTE = 2

-- sql  sql string 
-- callback:function(bsuccess,data), if success  data is result(json format) else error string
function Query(sql,callback)
	if thisService:GetMysqlService() == nil then
        return
    end

    if callback == nil then
        return 
    end

    local sm = Stream.new(256);
    sm:WriteUInt16(MsgID.MSG_S2S_MYSQL_OPERATION)
    sm:WriteInt16(QUERY)
    sm:WriteString(sql)

    thisService:SendRPC(thisService:GetMysqlService(),sm:Bytes(),function (data)
    	local br = BinaryReader.new(data)
    	callback(br:ReadInt16(),br:ReadString())
    end)
end

-- sql , sql string
-- data , data(json array foramt) for sql
-- order , sql param order, because lua hash json'key
-- callback , function(bsuccess,data) , if success data is nil, else error string 

--example
-- Execute("INSERT INTO test_table(col3,col1,col2) VALUES(?,?,?)",jsonstr,"col3,col1,col2",function (bsuccess,data)
--     if bsuccess then
--     else
--         Log.ConsoleTrace(data)
--     end
--     collectgarbage("collect")
-- end)    

function Execute(sql,data,order,callback)
	if thisService:GetMysqlService() == nil then
       	return
    end

    local sm = Stream.new(256);
    sm:WriteUInt16(MsgID.MSG_S2S_MYSQL_OPERATION)
    sm:WriteInt16(EXECUTE)
    sm:WriteString(sql)
    sm:WriteString(data)
    sm:WriteString(order)

    thisService:SendRPC(thisService:GetMysqlService(),sm:Bytes(),function (data)
    	local br = BinaryReader.new(data)
    	local success = true
    	local s = nil
    	if br:ReadInt16() == 0 then
    		success = false
    		s = br:ReadString()
    	end
    	callback(success,s)
    end)
end
