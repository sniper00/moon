local moon = require("moon")

local navmesh = require("navmesh")

---Navmesh file generated from https://github.com/recastnavigation/recastnavigation/tree/master/RecastDemo/Bin/Meshes

---static navmesh can load once, then shared by other threads
assert(navmesh.load_static("navmesh/dungeon_navmesh.bin"))
---

local negative_x_axis = 1 << 0
local negative_y_axis = 1 << 1
local negative_z_axis = 1 << 2

---create navmesh object
---local nav = navmesh.new("navmesh/dungeon_navmesh.bin", negative_x_axis|negative_z_axis)
local nav = navmesh.new("navmesh/dungeon_navmesh.bin", negative_z_axis)
local sx = 39
local sy = 10
local sz = 1.29

local ex = 20.88
local ey = 17.3
local ez = 78.44

---paths: {x1,y1,z1,x2,y2,z2,.....}
local paths, err = nav:find_straight_path(sx, sy, sz, ex, ey, ez)
assert(paths, err)
print_r(paths)

print(nav:random_position())

print(nav:random_position_around_circle(sx, sy, sz, 10))


local dynamic_nav = navmesh.new()

assert(dynamic_nav:load_dynamic("navmesh/nav_test_tilecache.bin"))

paths, err = dynamic_nav:find_straight_path(49.93, -2.43, 20.264, 55.84, -3.6692, -5.902)
assert(paths, err)
print_r(paths)

local px = 52
local py = -2.1996
local pz = 7.8510
local radius = 2
local height = 2
local oid = dynamic_nav:add_capsule_obstacle(px, py, pz, radius, height)
print(oid)

dynamic_nav:update(1.0)

paths, err = dynamic_nav:find_straight_path(49.93, -2.43, 20.264, 55.84, -3.6692, -5.902)
assert(paths, err)
print_r(paths)

assert(dynamic_nav:remove_obstacle(oid))
dynamic_nav:update(1.0)

paths, err = dynamic_nav:find_straight_path(49.93, -2.43, 20.264, 55.84, -3.6692, -5.902)
assert(paths, err)
print_r(paths)

moon.exit(100)