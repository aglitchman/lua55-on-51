if debug and not debug.getinfo and debug.info then
  debug.getinfo = function(level, what)
    what = what or "nSl"
    level = level + 1
    local info = {}
    if what:find("s") or what:find("S") then
      info.short_src = debug.info(level, "s")
      info.source = info.short_src
    end
    if what:find("l") then
      info.currentline = debug.info(level, "l")
    end
    if what:find("n") then
      info.name = debug.info(level, "n")
    end
    return info
  end
end
