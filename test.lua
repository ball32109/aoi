
local aoi = require "aoi"

local aoi_inst = aoi:new(120,120,2,20)

for i = 1 ,100 do
  aoi_inst:enter(i,{x = math.random(1,110),y = math.random(1,110)})
end

aoi_inst:enter(0,{x = 50,y = 50})

aoi_inst:update(0,{x = 55,y = 55})
aoi_inst:update(0,{x = 55,y = 55})
aoi_inst:update(0,{x = 55,y = 55})
aoi_inst:update(0,{x = 55,y = 55})
aoi_inst:update(0,{x = 55,y = 55})aoi_inst:update(0,{x = 55,y = 55})
