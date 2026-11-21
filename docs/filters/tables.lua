local kWidths = {"A", "B", "C", "D", "E", "F"}
local kNatural = 36
local kAligns = {AlignRight = "\\raggedleft", AlignCenter = "\\centering"}
local kMarks = {
    name = "\\biyosname",
    type = "\\biyostype",
    value = "\\biyosvalue",
    ["dir-in"] = "\\biyosdirin",
    ["dir-out"] = "\\biyosdirout",
    ["dir-inout"] = "\\biyosdirinout",
    see = "\\biyoslabel",
    also = "\\biyosalso",
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

local function mark(span)
    for class, macro in pairs(kMarks) do
        if span.classes:includes(class) then
            return wrapped(span, macro)
        end
    end
end

local function latex(blocks)
    local marked = pandoc.Blocks(blocks):walk({Span = mark})
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

local function natural(tbl, rows, column, count)
    if column == count or fixed(tbl) then
        return false
    end
    for _, cells in ipairs(rows) do
        local text = pandoc.utils.stringify(cells[column].contents)
        if utf8.len(text) > kNatural then
            return false
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

local function body_cell(cell)
    local text = latex(cell.contents)
    return text == "" and "" or ("\\leavevmode " .. text)
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
            for _, cells in ipairs(head and {head, table.unpack(rows)} or rows) do
                local format = cells == head and head_cell or body_cell
                lines[#lines + 1] = "\\biyoswiden" .. width .. "{" .. format(cells[column]) .. "}"
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

local function render(tbl)
    if #tbl.colspecs > #kWidths then
        return nil
    end
    local rows = rows_of(tbl)
    local head = tbl.head.rows[1] and tbl.head.rows[1].cells
    local caption = latex(tbl.caption.long)
    local lines = {"\\begin{biyostable}", sizes(tbl, rows, head)}
    lines[#lines + 1] = "\\begin{longtable}[l]{" .. columns(tbl) .. "}"
    for _, text in ipairs(header(tbl, head, caption)) do
        lines[#lines + 1] = text
    end
    lines[#lines + 1] = "\\endfirsthead"
    for _, text in ipairs(header(tbl, head, "")) do
        lines[#lines + 1] = text
    end
    lines[#lines + 1] = "\\endhead\n\\biyosbottomrule\\endfoot\n\\biyosbottomrule\\endlastfoot"
    for k, cells in ipairs(rows) do
        lines[#lines + 1] = (k > 1 and "\\biyosrule " or "") .. line(cells, body_cell)
    end
    lines[#lines + 1] = "\\end{longtable}\n\\end{biyostable}"
    return pandoc.RawBlock("latex", table.concat(lines, "\n"))
end

if quarto.doc.is_format("latex") then
    return {{Table = render}}
end
return {}
