require("functions")

local Stream = class("Stream")

local DEFAULT_CAPACITY = 64

function UIn16ToBytes(v)
	return string.pack("=H",v)
end

function Stream:ctor(capacity)
	capacity =capacity or DEFAULT_CAPACITY
	self.buf = {}
	self:Resize(capacity)
	self.wpos = 1
	self.rpos = 1
end

function Stream:Resize(size)
	for i = #self.buf+1,size do
			self.buf[i] = string.char(0)
	end
end

function Stream:WriteableSize()
	return #self.buf - (self.wpos -1)
end

function Stream:CheckWriteableSize(len)
	if self:WriteableSize() < len then
		self:MakeSpace(len)
	end
	assert(self:WriteableSize() >= len,"WriteableSize must >len")
end

function Stream:MakeSpace(len)
	if self:WriteableSize() + (self.rpos -1) < len then
		local s = DEFAULT_CAPACITY;
		while s < self.wpos -1 + len  do
			s = s * 2
		end
		print("Resize "..s)
		self:Resize(s)
	else
		print("move")
		local readable = self:Size()
		local j =1
		for i=self.rpos,self.wpos do
			self.buf[j] =self.buf[i]
		end
		self.rpos = 1;
		self.wpos = self.rpos + readable;
	end
end

function Stream:Size()
	return self.wpos - self.rpos
end

function Stream:Bytes()
	return table.concat(self.buf,"",self.rpos, self.rpos + self:Size()-1);
end

function Stream:Read(length)
	assert(length<=self:Size(),"read out of index")
	local s = table.concat(self.buf,"",self.rpos,self.rpos+length-1);
	self.rpos = self.rpos + length
	return s
end

function Stream:Write(s)
	local len = string.len(s)
	self:CheckWriteableSize(len)
	for i=1,len do
		self.buf[self.wpos] = string.sub(s,i,i)
		self.wpos = self.wpos +1
	end
end

function Stream:ReadInt16()
	return (string.unpack("=i2",self:Read(2)))
end

function Stream:WriteInt16(v)
	self:Write(string.pack("=i2",v)) 
	return self
end

function Stream:ReadUInt16()
	return (string.unpack("=I2",self:Read(2)))
end

function Stream:WriteUInt16(v)
	self:Write(string.pack("=I2",v)) 
	return self
end

function Stream:ReadInt32()
	return (string.unpack("=i4",self:Read(4)))
end

function Stream:WriteInt32(v)
	self:Write(string.pack("=i4",v)) 
	return self
end

function Stream:ReadUInt32()
	return (string.unpack("=I4",self:Read(4)))
end

function Stream:WriteUInt32(v)
	self:Write(string.pack("=I4",v)) 
	return self
end

function Stream:ReadInt64()
	return (string.unpack("=i8",self:Read(8)))
end

function Stream:WriteInt64(v)
	self:Write(string.pack("=i8",v)) 
	return self
end

function Stream:ReadUInt64()
	return (string.unpack("=I8",self:Read(8)))
end

function Stream:WriteUInt64(v)
	self:Write(string.pack("=I8",v)) 
	return self
end


function Stream:ReadFloat()
	return (string.unpack("=f",self:Read(4)))
end

function Stream:WriteFloat(v)
	self:Write(string.pack("=f",v)) 
	return self
end

function Stream:ReadDouble()
	return (string.unpack("=d",self:Read(8)))
end

function Stream:WriteDouble(v)
	self:Write(string.pack("=d",v)) 
	return self
end


function Stream:ReadString()
	local n = 0
	for i=self.rpos,#self.buf do
		if self.buf[i] == '\0' then
			break
		end
		n = n+1
	end
	local s = table.concat(self.buf,"",self.rpos,self.rpos+n-1)
	self.rpos = self.rpos+n+1
	return s
end

function Stream:WriteString(s)
	self:Write(string.pack("=z",s)) 
end


return Stream