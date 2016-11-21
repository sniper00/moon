
local protobuf 		= require "protobuf"
local Stream   		= require("Stream")
local BinaryReader 	= require("BinaryReader")

function UInt16ToBytes(v)
	return string.pack("=H",v)
end

function Serialize(msgid,pkg,t)
	assert(nil ~= t," must not be nil ")
	local encode = protobuf.encode(pkg,t)
	local s = Stream.new()
	s:WriteUInt16(msgid)
	s:Write(encode)
	return s:Bytes()
end

function Deserialize(pkg,data)
	return protobuf.decode(pkg,data)
end

function SerializeUserData(sessionid,accountid,playerid)
	 return string.pack("=I8I8I8",sessionid,accountid,playerid)
end

function DeserializeUserData(s)
	if 0 == string.len(s) then
		return nil
	end

	local br = BinaryReader.new(s)
	local u = {}
	u.sessionid = br:ReadUInt64()
	u.accountid = br:ReadUInt64()
	u.playerid = br:ReadUInt64()
	return u
end

function UnpackMsg(data)
	local msgid,n = string.unpack("=H",data)
	return msgid,string.sub(data,n)
end