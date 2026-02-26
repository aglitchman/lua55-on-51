do
  local bd = _G._json_basedir or ""
  local real_loadfile = loadfile
  local dirs = { bd .. "test/", bd .. "bench/", bd }
  loadfile = function(name)
    for _, d in ipairs(dirs) do
      local f, err = real_loadfile(d .. name)
      if f then return f, err end
    end
    return nil, "cannot find '" .. name .. "'"
  end
end
