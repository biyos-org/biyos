local function load(targets, path)
    local folder = pandoc.path.directory(quarto.doc.input_file)
    local file = io.open(pandoc.path.join({folder, path}))
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
local page = here()

local function indexes(meta)
    for _, path in ipairs(meta["name-indexes"]) do
        load(targets, pandoc.utils.stringify(path))
    end
end

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

return {{Meta = indexes}, {traverse = "topdown", Header = stop, Link = stop, Span = span, Code = code}}
