
local protobuf 		= require "protobuf"
local Stream   		= require("Stream")
local BinaryReader 	= require("BinaryReader")

function UInt16ToBytes(v)
	return string.pack("=H",v)
end

function Serialize(msgID,pkg,t)
	assert(nil ~= t," must not be nil ")
	local encode = protobuf.encode(pkg,t)
	return UInt16ToBytes(msgID)..encode
end

function Deserialize(pkg,data)
	return protobuf.decode(pkg,data)
end

function SerializeUserData(sessionid,accountid,playerid)
	 return string.pack("=I4I8I8",sessionid,accountid,playerid)
end

function DeserializeUserData(s)
	if 0 == string.len(s) then
		return nil
	end

	local br = BinaryReader.new(s)
	local u = {}
	u.sessionid = br:ReadUInt32()
	u.accountid = br:ReadUInt64()
	u.playerid = br:ReadUInt64()
	return u
end

function UnpackMsg(data)
	local msgID,n = string.unpack("=H",data)
	return msgID,string.sub(data,n)
end