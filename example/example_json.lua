local moon = require("moon")
local json = require("json")

print("outout t1:", json.encode({
    [1] = "a",
    [2] = "b",
    [100] = "c",
})) -- {"100":"c","1":"a","2":"b"}

print("outout t2:", json.encode({
    [1] = "a",
    [2] = "b",
    [5] = "c",
})) -- {"1":"a","2":"b","5":"c"}

print("outout t3:", json.encode({
    [1] = "a",
    [2] = "b",
    [3] = "c",
})) --  ["a","b","c"]

print("outout t4:", json.encode({
	"a","b",["c"] = 1
})) --  {"1":"a","2":"b","c":1}

print("outout t5:", json.encode({
	1, 2, 3,4, ["a"]=1,["b"]=2,["c"] =3,
})) --  {"1":1,"2":2,"3":3,"4":4,"b":2,"a":1,"c":3}

print("outout t6:", json.encode({
	["a"]=1,["b"]=2,["c"] =3,
})) --  {"b":2,"a":1,"c":3}

print("outout t7:", json.encode({
	["1"]=1,["2"]=2,["3"] =3,
})) --  {"2":2,"1":1,"3":3}

print("outout t8:", json.encode({
	["1.0"]=1,["2.0"]=2,["3.0"] =3,
})) --  {"1.0":1,"3.0":3,"2.0":2}

print("outout t9:", json.encode({
	[2]="a",[1] = "b"
})) --  {"2":"a","1":"b"}

print("outout t10:", json.encode({
	["100abce"] = "hello"
})) -- {"100abce":"hello"}