function CreateOrdered_0(initial)
  local _data = {}
  local _order = {}
  local self = {}

  local mt = {
    __newindex = function(_, k, v)
      if not _data[k] then
        table.insert(_order, k)
      end
      _data[k] = v
    end,
    __index = {
      pairs = function() return pairs(_data) end,
      opairs = function()
        local idx = 0
        return function()
          local key
          repeat
            idx = idx + 1
            key = _order[idx]
          until _data[key] or idx > #_order
          return key, key and _data[key]
        end
      end,
    }
  }
  setmetatable(self, mt)
  if initial then
    for k, v in pairs(initial) do
      self[k] = v
    end
  end
  return self
end

--------------------------------------------------------------------------

function CreateOrdered_1(initial)
  local _data = {}       -- Key-value storage
  local _order = {}      -- Insertion order tracking
  local _exists = {}     -- Tracks existing keys (for O(1) existence checks)

  local mt = {
    __newindex = function(_, k, v)
      if v == nil then
        -- Handle deletion
        _data[k] = nil
        _exists[k] = nil
      else
        if not _exists[k] then
          -- Add new key to order tracking
          _exists[k] = true
          table.insert(_order, k)
        end
        _data[k] = v
      end
    end,

    __index = {
      pairs = function() 
        return pairs(_data) 
      end,
      opairs = function()
        local i = 0
        return function()
          local key
          repeat
            i = i + 1
            key = _order[i]
          until not key or _exists[key]
          return key, key and _data[key]
        end
      end,
    }
  }

  local self = setmetatable({}, mt)
  if initial then
    for k, v in pairs(initial) do
      self[k] = v
    end
  end
  return self
end

--------------------------------------------------------------------------


-- Helper functions
local function time(func)
  local start = os.clock()
  func()
  return os.clock() - start
end

local function verifyOrder(t, expectedKeys)
  local i = 1
  for k in t:opairs() do
    assert(k == expectedKeys[i], "Order verification failed")
    i = i + 1
  end
  assert(i-1 == #expectedKeys, "Order count mismatch " .. i - 1 .. ":" .. #expectedKeys)
end

local function verifyContent(t, expected)
  for k, v in pairs(expected) do
    assert(t[k] == v, string.format("Mismatch at key '%s'", tostring(k)))
  end
end

-- Comprehensive benchmark
local function runBenchmarks(createFunc, name)
  local results = {name = name}
  local testSize = 100000
  local repetitions = 100

  -- Warm-up and GC control
  collectgarbage()
  for _ = 1, 1000 do createFunc() end

  -- Test 1: Insertion performance
  results.insert = time(function()
    for _ = 1, repetitions do
      local t = createFunc()
      for i = 1, testSize do
        t[i] = i
      end
      verifyContent(t, t._data or t)  -- Handle different impls
    end
  end) / repetitions

  -- Test 2: Random access
  results.access = time(function()
    local t = createFunc()
    for i = 1, testSize do t[i] = i end
    for _ = 1, repetitions do
      for i = 1, testSize do
        local _ = t[math.random(testSize)]
      end
    end
  end) / repetitions

  -- Test 3: Deletion performance
  results.delete = time(function()
    for _ = 1, repetitions do
      local t = createFunc()
      for i = 1, testSize do t[i] = i end
      for i = 1, testSize, 2 do
        t[i] = nil
      end
      verifyOrder(t, (function()
        local keys = {}
        for i = 2, testSize, 2 do keys[#keys+1] = i end
        return keys
      end)())
    end
  end) / repetitions

  -- Test 4: Unordered iteration
  results.upairs = time(function()
    local t = createFunc()
    for i = 1, testSize do t[i] = i end
    for _ = 1, repetitions do
      for _ in pairs(t) do end
    end
  end) / repetitions

  -- Test 5: Ordered iteration
  results.opairs = time(function()
    local t = createFunc()
    for i = 1, testSize do t[i] = i end
    for _ = 1, repetitions do
      for _ in t:opairs() do end
    end
  end) / repetitions

  return results
end

-- Compare multiple implementations
local function compareImplementations(...)
  local implementations = {...}
  local headers = {(function(...)
      local names = {}
      for _, v in pairs(...) do
        if v.name then names[#names+1] = v.name end
      end
      return table.unpack(names)
    end)(implementations)}
  
  print(string.format(("%-12s | "):rep(#headers + 1), "Test", table.unpack(headers)))
  print(string.rep("-", 14 + 14*#implementations + (#headers % 2 == 0 and 2 or 3)))
  
  local tests = {"insert", "access", "delete", "upairs", "opairs"}
  for _, test in ipairs(tests) do
    io.write(string.format("%-12s | ", test))
    for _, impl in ipairs(implementations) do
      io.write(string.format("%-10.2fμs | ", impl[test] * 1e6))
    end
    print()
  end
end

-- Execute benchmarks
local impl1 = runBenchmarks(function() return CreateOrdered_0() end, "Impl 0")
local impl2 = runBenchmarks(function() return CreateOrdered_1() end, "Impl 1")

compareImplementations(impl1, impl2)
