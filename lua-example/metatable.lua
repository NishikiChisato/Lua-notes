-- this table act as metatable for new created table
local point = {__name = "point"}
point.__index = point

-- if we want to create a new instance by point(...), we need set metatable(__call) for this table
setmetatable(point, {
  __call = function(func, ...)
    return func:new(...)
  end
})

-- these three functions regist in point table
function point:new(x, y)
	local self = setmetatable({x = (x or 0), y = (y or 0)}, point)
	return self
end

function point:inc(xinc, yinc)
	self.x = self.x + xinc
	self.y = self.y + yinc
end

function point:dis()
	print(string.format("x: %d, y : %d", self.x, self.y))
end

-- metatable for line class
local line = {__name = "line"}
line.__index = line
setmetatable(line, {
	-- search in point when necessary
	__index = point,
	__call = function(func, ...)
		return func:new(...)
	end
})

function line:new(l)
	local self = setmetatable({x = point(l.xx, l.xy), y = point(l.yx, l.yy)}, line)
	return self
end

function line:inc(pos, xinc, yinc)
	if pos == 0 then
		self.x:inc(xinc, yinc)
	elseif pos == 1 then
		self.y:inc(xinc, yinc)
	else
		error("type error")
	end
end

function line:dis()
	self.x:dis()
	self.y:dis()
end

function line:pos(pos)
	if pos == 0 then
		return self.x
	elseif pos == 1 then
		return self.y
	else
		error("type error")
end
end

local l = line {xx = 1, xy = 2, yx = 11, yy = 12}
l:dis()
