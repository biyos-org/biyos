local kIndexes = {"reference/_capi/names.json", "reference/_options/names.json"}

local function load(targets, path)
    local file = io.open(pandoc.path.join({quarto.project.directory, path}))
    if not file then
        return
    end
    for name, target in pairs(quarto.json.decode(file:read("a"))) do
        targets[name] = target
    end
    file:close()
end

local function here()
    local project = quarto.project.directory .. "/"
    return quarto.doc.input_file:sub(#project + 1)
end

local function href(target, page)
    local file, anchor = target:match("^(.-)(#.*)$")
    if file == page then
        return anchor
    end
    local _, depth = page:gsub("/", "")
    return string.rep("../", depth) .. target
end

local targets = {}
for _, path in ipairs(kIndexes) do
    load(targets, path)
end
local page = here()

local function stop(element)
    return element, false
end

local function span(element)
    if element.identifier ~= "" then
        return element, false
    end
end

local function code(element)
    local target = targets[element.text]
    if target then
        return pandoc.Link(element, href(target, page)), false
    end
end

return {{traverse = "topdown", Header = stop, Link = stop, Span = span, Code = code}}
