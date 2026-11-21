local function title_of(div)
    local first = div.content[1]
    if first and first.t == "Header" then
        div.content:remove(1)
        return pandoc.write(pandoc.Pandoc({pandoc.Plain(first.content)}), "latex")
    end
    return ""
end

local function note(div)
    if not div.classes:includes("callout-note") then
        return nil
    end
    local title = title_of(div)
    local blocks = pandoc.Blocks({pandoc.RawBlock("latex", "\\begin{biyosnote}{" .. title .. "}")})
    blocks:extend(div.content)
    blocks:insert(pandoc.RawBlock("latex", "\\end{biyosnote}"))
    return blocks
end

if quarto.doc.is_format("latex") then
    return {{Div = note}}
end
return {}
