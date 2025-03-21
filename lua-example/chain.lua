local chain = {}

function chain.create(value)
  local inner = {
    _val = value
  }

  local function add(func, inner, inc)
    inner._val = inner._val + inc
    return inner
  end

  local function mul(func, inner, inc)
    inner._val = inner._val * inc
    return inner
  end

  local function value(func, inner) return inner._val end

  return setmetatable(inner, {
    -- 'inner' table don't have these function, we should access by setting __index
    __index = {
      -- in order to directly call it with add(...), we need set __call for this table instead of directly set a function to this field
      add = setmetatable({}, { __call = add, }),
      mul = setmetatable({}, { __call = mul, }),
      value = setmetatable({}, { __call = value, }),
    }
  })
end

-- 20
print(chain.create(2):add(3):add(5):mul(2):value())
-- nil
print(chain.create():value())
