require("functions")

local BinaryReader = class("BinaryReader")

function BinaryReader:ctor(s)
	self.buf = s
	self.rpos = 1
end

function BinaryReader:Size()
	return string.len(self.buf) - (self.rpos-1)
end

function BinaryReader:Bytes()
	return string.sub(self.buf,self.rpos, self.rpos + self:Size()-1);
end

function BinaryReader:Read(length)
	assert(length<=self:Size(),"read out of index")
	local s = string.sub(self.buf,self.rpos,self.rpos+length-1);
	self.rpos = self.rpos + length
	return s
end

function BinaryReader:ReadInt16()
	return (string.unpack("=i2",self:Read(2)))
end

function BinaryReader:ReadUInt16()
	return (string.unpack("=I2",self:Read(2)))
end

function BinaryReader:ReadInt32()
	return (string.unpack("=i4",self:Read(4)))
end

function BinaryReader:ReadUInt32()
	return (string.unpack("=I4",self:Read(4)))
end

function BinaryReader:ReadInt64()
	return (string.unpack("=i8",self:Read(8)))
end

function BinaryReader:ReadUInt64()
	return (string.unpack("=I8",self:Read(8)))
end

function BinaryReader:ReadFloat()
	return (string.unpack("=f",self:Read(4)))
end

function BinaryReader:ReadDouble()
	return (string.unpack("=d",self:Read(8)))
end

function BinaryReader:ReadString()
	local n = 0
	for i=self.rpos,#self.buf do
		if self.buf:sub(i,i) == '\0' then
			break
		end
		n = n+1
	end
	local s = string.sub(self.buf,self.rpos,self.rpos+n-1)
	self.rpos = self.rpos+n+1
	return s
end

return BinaryReader