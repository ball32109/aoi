
local aoi = require "aoi"

local aoi_inst = aoi:new(120,120,2,20)

for i = 1 ,100 do
  aoi_inst:enter(i,{x = math.random(1,110),y = math.random(1,110)})
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

function move(id,pos)
  local enter_self,enter_other,leave_self,leave_other = aoi_inst:update(0,{x = pos.x,y = pos.y})
  print("enter_self")
  dump(enter_self)
  print("enter_other")
  dump(enter_other)
  print("leave_self")
  dump(leave_self)
  print("leave_other")
  dump(leave_other)
end

move(0,{x = 59,y = 59})
move(0,{x = 65,y = 65})
move(0,{x = 75,y = 75})
move(0,{x = 85,y = 85})
move(0,{x = 15,y = 55})
