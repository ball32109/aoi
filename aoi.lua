local c = require "aoi.c"

local aoi = {}

function aoi:new(width,high,tilelen,level_count)
  local ctx = setmetatable({}, {
              __index = self,
              __gc = function (ctx)
                for k,v in pairs(ctx) do
                  if k == "core" then
                    c.aoi_delete(v)
                  end
                end
              end
          }
        )
  
  ctx.core = c.aoi_new(width,high,tilelen)
  ctx.obj_mgr = {}
  ctx.obj_count = 0
  ctx.level_count = level_count or 0
  return ctx
end

function aoi:enter(id,pos)
  local level_index = self.obj_count / self.level_count + 1
  local cobj,enter_self,enter_other = c.aoi_enter(self.core,id,pos.x,pos.y,pos.x,pos.y,level_index,4)
  self.obj_count = self.obj_count + 1
  self.obj_mgr[id] = {pos = {x = pos.x,y = pos.y},cobj = cobj}
  return enter_self,enter_other
end

function aoi:leave(id)
  local info = self.obj_mgr[id]
  local leave_other = c.aoi_leave(self.core,info.cobj)
  self.obj_mgr[id] = nil
  self.obj_count = self.count - 1
  return leave_other
end

function aoi:update(id,np)
  local info = self.obj_mgr[id]
  local op = {x = info.pos.x,y = info.pos.y}
  info.pos.x = np.x
  info.pos.y = np.y
  return c.aoi_update(self.core,info.cobj,op.x,op.y,np.x,np.y)
end

function aoi:viewlist(id)
  local info = self.obj_mgr[id]
  return c.aoi_viewlist(self.core,info.cobj)
end

return aoi
