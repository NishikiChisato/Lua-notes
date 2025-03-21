local listset = {__name = "listset"}
setmetatable(listset, {
  __call = function(func, ...)
    return func:new(...)
  end,
})

function listset:new(list)
  self[#self + 1] = list
  return self
end

function listset:dump()
  print("len: ", #self)
  local lines = {}
  for i = 1, #self do
    local line = {}
    for j = 1, #self[i] do
      table.insert(line, self[i][j])
    end
    table.insert(lines, table.concat(line, ", "))
  end
  print(table.concat(lines, "\n"))
end

function listset:foreach()
  local row = #self
  local i, j = 1, 1
  return function()
    if i > #self then return nil end
    if not self[i] or j > #self[i] then j = 1; i = i + 1 end
    if i <= #self and (self[i] and j <= #self[i]) then
      local ri, rj, rv = i, j, self[i][j]
      j = j + 1
      return ri, rj, rv
    end
  end
end

listset {1,2,3} {2,3,4} {3,4,5} {4,5,6} {5,6,7} : dump()

for i, j, v in listset 
  {"v1", "v2", "v3"} 
  {"v4", "v5", "v6"} 
  {"v7", "v8", "v9"} 
  : foreach() do
  print(i, j, v)
end
