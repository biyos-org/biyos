local kWidths = {"A", "B", "C", "D", "E", "F"}
local kNatural = 36
local kShort = 12
local kAligns = {AlignRight = "\\raggedleft", AlignCenter = "\\centering"}
local kMarks = {
    name = "\\biyosname",
    type = "\\biyostype",
    value = "\\biyosvalue",
    dir = "\\biyosdir",
    see = "\\biyoslabel",
    also = "\\biyosalso",
    unbroken = "\\biyosunbroken",
}

local function wrapped(span, macro)
    local out = pandoc.Inlines({})
    if span.identifier ~= "" then
        out:insert(pandoc.RawInline("latex", "\\phantomsection\\label{" .. span.identifier .. "}"))
    end
    out:insert(pandoc.RawInline("latex", macro .. "{"))
    out:extend(span.content)
    out:insert(pandoc.RawInline("latex", "}"))
    return out
end

local function bracket(str)
    if str.text:find("[", 1, true) then
        local out = pandoc.Inlines({})
        for piece in str.text:gmatch("[^%[]*%[?") do
            if piece ~= "" then
                out:insert(pandoc.Str(piece))
                if piece:sub(-1) == "[" then
                    out:insert(pandoc.RawInline("latex", "\\allowbreak{}"))
                end
            end
        end
        return out
    end
end

local function tied(inlines)
    for k = 2, #inlines do
        if inlines[k].t == "Space" and inlines[k - 1].t == "Str" and inlines[k - 1].text == "|" then
            inlines[k] = pandoc.RawInline("latex", "~")
        end
    end
    return inlines
end

local function breakable(span)
    return span:walk({Str = bracket, Inlines = tied})
end

local function marked(span)
    for class, macro in pairs(kMarks) do
        if span.classes:includes(class) then
            return wrapped(span, macro)
        end
    end
end

local function mark(span)
    if span.classes:includes("type") then
        span = breakable(span)
    end
    local out = marked(span)
    if span.classes:includes("under") then
        out = out or pandoc.Inlines(span.content)
        out:insert(1, pandoc.RawInline("latex", "\\newline{}"))
    end
    return out
end

local function is_span(inline, class)
    return inline and inline.t == "Span" and inline.classes:includes(class)
end

local function spaced(inlines)
    for k = 2, #inlines - 1 do
        if inlines[k].t == "Space" and is_span(inlines[k - 1], "name") and is_span(inlines[k + 1], "type") then
            inlines[k] = pandoc.RawInline("latex", "\\enskip{}")
        end
    end
    return inlines
end

local function latex(blocks)
    local marked = pandoc.Blocks(blocks):walk({Inlines = spaced}):walk({Span = mark})
    local text = pandoc.write(pandoc.Pandoc(marked), "latex")
    return (text:gsub("^%s+", ""):gsub("%s+$", ""):gsub("\n\n+", "\\par "):gsub("\n", " "))
end

local function rows_of(tbl)
    local rows = {}
    for _, body in ipairs(tbl.bodies) do
        for _, row in ipairs(body.body) do
            rows[#rows + 1] = row.cells
        end
    end
    return rows
end

local function fixed(tbl)
    for _, colspec in ipairs(tbl.colspecs) do
        if not colspec[2] or colspec[2] <= 0 then
            return false
        end
    end
    return true
end

local function split(cell)
    local parts = {}
    local function take(span)
        if span.classes:includes("under") then
            local plain = pandoc.Span(span.content, pandoc.Attr("", {"type"}))
            parts[#parts + 1] = {pandoc.Plain({plain})}
            return {}
        end
    end
    table.insert(parts, 1, pandoc.Blocks(cell.contents):walk({Span = take}))
    return parts
end

local function natural(tbl, rows, column, count)
    if column == count or fixed(tbl) then
        return false
    end
    for _, cells in ipairs(rows) do
        for _, part in ipairs(split(cells[column])) do
            if utf8.len(pandoc.utils.stringify(part)) > kNatural then
                return false
            end
        end
    end
    return true
end

local function shares(tbl, flexible)
    local out, total = {}, 0
    for _, column in ipairs(flexible) do
        out[column] = math.max(tbl.colspecs[column][2] or 0, 0)
        total = total + out[column]
    end
    for _, column in ipairs(flexible) do
        out[column] = total > 0 and out[column] / total or 1 / #flexible
    end
    return out
end

local function head_cell(cell)
    return "\\leavevmode\\biyoshead{" .. latex(cell.contents) .. "}"
end

local function body_text(blocks)
    local text = latex(blocks)
    return text == "" and "" or ("\\leavevmode " .. text)
end

local function body_cell(cell)
    return body_text(cell.contents)
end

local function widen(width, text)
    return "\\biyoswiden" .. width .. "{" .. text .. "}"
end

local function line(cells, format)
    local out = {}
    for _, cell in ipairs(cells) do
        out[#out + 1] = format(cell)
    end
    return table.concat(out, " & ") .. " \\\\"
end

local function sizes(tbl, rows, head)
    local count = #tbl.colspecs
    local lines, flexible, used = {"\\biyosreset"}, {}, {}
    for column = 1, count do
        local width = "\\biyosw" .. kWidths[column]
        if natural(tbl, rows, column, count) then
            used[#used + 1] = width
            if head then
                lines[#lines + 1] = widen(width, head_cell(head[column]))
            end
            for _, cells in ipairs(rows) do
                for _, part in ipairs(split(cells[column])) do
                    lines[#lines + 1] = widen(width, body_text(part))
                end
            end
            lines[#lines + 1] = "\\biyoscap" .. width
        else
            flexible[#flexible + 1] = column
        end
    end
    local rest = "\\linewidth-" .. (2 * count) .. "\\tabcolsep"
    for _, width in ipairs(used) do
        rest = rest .. "-" .. width
    end
    lines[#lines + 1] = "\\setlength{\\biyosrest}{\\dimexpr " .. rest .. "\\relax}"
    local share = shares(tbl, flexible)
    for _, column in ipairs(flexible) do
        local width = "\\biyosw" .. kWidths[column]
        lines[#lines + 1] = string.format("\\setlength{%s}{%.3f\\biyosrest}", width, share[column])
    end
    return table.concat(lines, "\n")
end

local function columns(tbl)
    local out = {}
    for column, colspec in ipairs(tbl.colspecs) do
        local align = kAligns[colspec[1]] or "\\raggedright"
        out[#out + 1] = ">{" .. align .. "\\arraybackslash}p{\\biyosw" .. kWidths[column] .. "}"
    end
    return table.concat(out)
end

local function header(tbl, head, caption)
    local lines = {}
    if caption ~= "" then
        local count = #tbl.colspecs
        lines[#lines + 1] = "\\multicolumn{" .. count .. "}{@{}l@{}}{\\biyoscaption{" .. caption .. "}} \\\\"
    end
    if head then
        lines[#lines + 1] = "\\biyosheadrow " .. line(head, head_cell) .. " \\biyosheadrule"
    end
    return lines
end

local function body(rows)
    local lines = {}
    for k, cells in ipairs(rows) do
        lines[#lines + 1] = (k > 1 and "\\biyosrule " or "") .. line(cells, body_cell)
    end
    return lines
end

local function append(lines, more)
    table.move(more, 1, #more, #lines + 1, lines)
end

local function long(tbl, head, caption)
    local lines = {"\\begin{longtable}[l]{" .. columns(tbl) .. "}"}
    append(lines, header(tbl, head, caption))
    lines[#lines + 1] = "\\endfirsthead"
    append(lines, header(tbl, head, ""))
    lines[#lines + 1] = "\\endhead\n\\biyosbottomrule\\endfoot\n\\biyosbottomrule\\endlastfoot"
    return "biyostable", lines, "\\end{longtable}"
end

local function short(tbl, head, caption)
    local lines = {"\\noindent\\begin{tabular}{" .. columns(tbl) .. "}"}
    append(lines, header(tbl, head, caption))
    return "biyoskept", lines, "\\biyosbottomrule\n\\end{tabular}"
end

local function render(tbl)
    if #tbl.colspecs > #kWidths then
        return nil
    end
    local rows = rows_of(tbl)
    local head = tbl.head.rows[1] and tbl.head.rows[1].cells
    local caption = latex(tbl.caption.long)
    local build = #rows <= kShort and short or long
    local environment, opening, closing = build(tbl, head, caption)
    local lines = {"\\begin{" .. environment .. "}", sizes(tbl, rows, head)}
    append(lines, opening)
    append(lines, body(rows))
    lines[#lines + 1] = closing .. "\n\\end{" .. environment .. "}"
    return pandoc.RawBlock("latex", table.concat(lines, "\n"))
end

if quarto.doc.is_format("latex") then
    return {{Table = render}}
end
return {}
