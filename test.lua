
local aoi = require "aoi"

local aoi_inst = aoi:new(120,120,2,100000)

local poses = {}
for i = 1 ,10000 do
  local pos = {x = math.random(1,110),y = math.random(1,110)}
  table.insert(poses,pos)
  aoi_inst:enter(i,pos)
end

function dump(t)
  for k,v in pairs(t) do
    print(v)
  end
end

local enter_self,enter_other = aoi_inst:enter(0,{x = 50,y = 50})
print("enter_self")
dump(enter_self)
print("enter_other")
dump(enter_other)
print("@@@@@@@@@@@@@@@@@")

function update(id,pos)
  local enter_self,enter_other,leave_self,leave_other = aoi_inst:update(id,{x = pos.x,y = pos.y})
  print("enter_self")
  dump(enter_self)
  print("enter_other")
  dump(enter_other)
  print("leave_self")
  dump(leave_self)
  print("leave_other")
  dump(leave_other)
end

local begin = {x = 1,y = 1}
local over = {x = 109,y = 109}
local speed = 1

function sleep(n)
   os.execute("sleep " .. n)
end

function move()
  while true do
    begin.x = begin.x + 2
    begin.y = begin.y + 2
    update(0,begin)
    sleep(1)
  end
end

local viewlist = aoi_inst:viewlist(0)
print("viewlist")
dump(viewlist)

