local kLabels = {since = "\\biyossince{", see = "\\biyoslabel{"}

local function label(span, first)
    for class, opening in pairs(kLabels) do
        if span.classes:includes(class) then
            local lead = (class == "see" and not first) and "\\quad" or ""
            local out = pandoc.Inlines({pandoc.RawInline("latex", lead .. opening)})
            out:extend(span.content)
            out:insert(pandoc.RawInline("latex", "}"))
            return out
        end
    end
    return pandoc.Inlines({span})
end

local function line(inlines)
    local out = pandoc.Inlines({pandoc.RawInline("latex", "\\leavevmode{\\color{muted}")})
    local first = true
    for _, inline in ipairs(inlines) do
        if inline.t == "Span" then
            out:extend(label(inline, first))
        else
            out:insert(inline)
        end
        first = first and (inline.t == "Space" or inline.t == "SoftBreak")
    end
    out:insert(pandoc.RawInline("latex", "}"))
    return out
end

local function meta(div)
    if not div.classes:includes("meta") then
        return nil
    end
    for _, block in ipairs(div.content) do
        if block.t == "Para" or block.t == "Plain" then
            block.content = line(block.content)
        end
    end
    local blocks = pandoc.Blocks({pandoc.RawBlock("latex", "\\begin{biyosmeta}")})
    blocks:extend(div.content)
    blocks:insert(pandoc.RawBlock("latex", "\\end{biyosmeta}"))
    return blocks
end

if quarto.doc.is_format("latex") then
    return {{Div = meta}}
end
return {}
