local kEscapes = {["&"] = "&amp;", ["<"] = "&lt;", [">"] = "&gt;", ['"'] = "&quot;"}

local function escaped(text)
    return (text:gsub('[&<>"]', kEscapes))
end

local function pieces(text)
    local out = {}
    for run, gap in text:gmatch("([^ ]*)( *)") do
        local parts = {}
        for part in run:gmatch("[^_]*_?") do
            if part ~= "" then
                parts[#parts + 1] = '<span class="run">' .. escaped(part) .. "</span>"
            end
        end
        out[#out + 1] = table.concat(parts, "<wbr>") .. gap
    end
    return table.concat(out)
end

local function code(element)
    return pandoc.RawInline("html", "<code>" .. pieces(element.text) .. "</code>")
end

local function bracket(element)
    if element.text:find("[", 1, true) then
        return pandoc.RawInline("html", (escaped(element.text):gsub("%[", "[<wbr>")))
    end
end

local function span(element)
    if element.classes:includes("type") then
        return element:walk({Str = bracket})
    end
end

if quarto.doc.is_format("html") then
    return {{Code = code, Span = span}}
end
return {}
