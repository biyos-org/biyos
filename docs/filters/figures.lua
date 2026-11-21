local kEscapes = {["&"] = "&amp;", ["<"] = "&lt;", [">"] = "&gt;", ['"'] = "&quot;"}

local function inline(image)
    local source = image.src
    if not source:match("%.svg$") or not source:match("figures/") then
        return nil
    end
    local directory = pandoc.path.directory(quarto.doc.input_file)
    local file = assert(io.open(pandoc.path.join({directory, source})))
    local text = file:read("a")
    file:close()
    local label = pandoc.utils.stringify(image.caption):gsub('[&<>"]', kEscapes)
    local opening = '<svg class="diagram" role="img" aria-label="' .. label .. '" '
    text = text:gsub("^%s*<%?xml.-%?>%s*", "")
    text = text:gsub("<svg ", (opening:gsub("%%", "%%%%")), 1)
    return pandoc.RawInline("html", text)
end

if quarto.doc.is_format("html") then
    return {{Image = inline}}
end
return {}
