
local protobuf = require "protobuf"

local SerializeUtil = {}

SerializeUtil.Serialize = function (msgID,pkg,t)
	local msg = CreateMessage()
	assert(nil ~= t," must not be nil ")
	encode = protobuf.encode(pkg,t)
	msg:WriteData(UIn16ToBytes(msgID))
	msg:WriteData(encode)
	return msg
end

SerializeUtil.SerializeEx = function (msgID)
	local msg = CreateMessage()
	msg:WriteData(UIn16ToBytes(msgID))
	return msg
end

SerializeUtil.Deserialize = function(pkg,data)
	return protobuf.decode(pkg,data:Bytes())
end

SerializeUtil.Deserialize = function(pkg,data)
	return protobuf.decode(pkg,data:Bytes())
end

return SerializeUtil