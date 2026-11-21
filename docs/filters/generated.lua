local function read(path)
    local file = io.open(pandoc.path.join({quarto.project.directory, path}))
    if not file then
        error("no generated file " .. path .. ": run the pre-render scripts")
    end
    local text = file:read("a")
    file:close()
    return text
end

local function mark(header)
    if header.level > 2 then
        header.classes:insert("unnumbered")
    end
    if header.level > 3 then
        header.classes:insert("unlisted")
    end
    return header
end

function Div(div)
    local path = div.attributes.source
    if not path then
        return nil
    end
    local blocks = pandoc.read(read(path), "markdown").blocks
    return blocks:walk({Header = mark})
end
