local kWidth = 76

local function root()
    return pandoc.path.directory(quarto.project.directory)
end

local function read(path)
    local file = io.open(pandoc.path.join({root(), path}))
    if not file then
        error("no file " .. path)
    end
    local lines = {}
    for line in file:lines() do
        lines[#lines + 1] = line
    end
    file:close()
    return lines
end

local function opens(line, name)
    if line:match("^%s") or line == "" then
        return false
    end
    return line:find("%f[%w_]" .. name .. "%f[^%w_]") ~= nil
end

local function last_c(lines, first)
    if lines[first]:match(";$") then
        return first
    end
    for k = first + 1, #lines do
        if lines[k]:match("^}") then
            return k
        end
    end
    return #lines
end

local function last_python(lines, first)
    local last = first
    for k = first + 1, #lines do
        local line = lines[k]
        if line ~= "" and not line:match("^%s") then
            break
        end
        if line ~= "" then
            last = k
        end
    end
    return last
end

local function item(lines, name, python, path)
    for k, line in ipairs(lines) do
        if opens(line, name) then
            local last = python and last_python(lines, k) or last_c(lines, k)
            return table.move(lines, k, last, 1, {})
        end
    end
    error(path .. " has no item " .. name)
end

local function select(lines, names, python, path)
    local parts = {}
    for name in names:gmatch("%S+") do
        parts[#parts + 1] = table.concat(item(lines, name, python, path), "\n")
    end
    return table.concat(parts, "\n\n")
end

local function check(text, path)
    for line in (text .. "\n"):gmatch("(.-)\n") do
        if utf8.len(line) > kWidth then
            quarto.log.warning(path .. ": a line of " .. utf8.len(line) .. " characters")
        end
    end
end

function CodeBlock(block)
    local path = block.attributes.file
    if not path then
        return nil
    end
    local lines = read(path)
    local names = block.attributes.item
    local python = block.classes:includes("python")
    local text = names and select(lines, names, python, path) or table.concat(lines, "\n")
    check(text, path)
    block.attributes.file = nil
    block.attributes.item = nil
    block.attributes.filename = path
    block.text = text
    return block
end
