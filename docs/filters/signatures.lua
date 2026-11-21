local kKeywords = {const = true, void = true, struct = true, typedef = true}
local kHtml = {kw = "kw", type = "dt", name = "fu", param = "va", default = "cn"}
local kLatex = {
    kw = "KeywordTok",
    type = "DataTypeTok",
    name = "FunctionTok",
    param = "VariableTok",
    default = "ConstantTok",
}
local kHtmlEscapes = {["&"] = "&amp;", ["<"] = "&lt;", [">"] = "&gt;"}
local kLatexEscapes = {
    ["\\"] = "\\textbackslash{}",
    ["{"] = "\\{",
    ["}"] = "\\}",
    ["_"] = "\\_",
    ["%"] = "\\%",
    ["#"] = "\\#",
    ["&"] = "\\&",
    ["$"] = "\\$",
}

local function tokens(text)
    local out = {}
    local pos = 1
    while pos <= #text do
        local first, last = text:find("^[%a_][%w_]*", pos)
        local kind = "word"
        if not first then
            first, last = text:find("^%s+", pos)
            kind = "space"
        end
        if not first then
            first, last = pos, pos
            kind = "mark"
        end
        out[#out + 1] = {kind = kind, text = text:sub(first, last)}
        pos = last + 1
    end
    return out
end

local function opening(list, from)
    for k = from, #list do
        if list[k].text == "(" then
            return k
        end
    end
end

local function role(word)
    return kKeywords[word] and "kw" or "type"
end

local function last_word(list, from, to)
    for k = to, from, -1 do
        if list[k].kind == "word" then
            return k
        end
    end
end

local function pointed(list)
    for k = 2, #list - 1 do
        if list[k].text == "*" and list[k - 1].text == "(" and list[k + 1].kind == "word" then
            return k + 1
        end
    end
end

local function typedef(list)
    local name = pointed(list) or last_word(list, 1, #list)
    for k = 1, name - 1 do
        if list[k].kind == "word" then
            list[k].role = role(list[k].text)
        end
    end
    list[name].role = "name"
    return pointed(list) and opening(list, name)
end

local function head(list, open, python)
    local name = last_word(list, 1, open - 1)
    for k = 1, open - 1 do
        local token = list[k]
        if k == name then
            token.role = "name"
        elseif token.kind == "word" and not python then
            token.role = role(token.text)
        end
    end
end

local function c_parameter(list, first, last)
    local param = last_word(list, first, last)
    for k = first, last do
        if list[k].kind == "word" then
            list[k].role = k == param and "param" or role(list[k].text)
        end
    end
end

local function python_parameter(list, first, last)
    local after = false
    for k = first, last do
        local token = list[k]
        if token.text == "=" then
            after = true
        elseif after and token.kind ~= "space" then
            token.role = "default"
        elseif token.kind == "word" then
            token.role = "param"
        end
    end
end

local function parameters(list, open, python)
    local each = python and python_parameter or c_parameter
    local depth, first = 0, open + 1
    for k = open, #list do
        local text = list[k].text
        if text == "(" then
            depth = depth + 1
        elseif text == ")" or (text == "," and depth == 1) then
            if depth == 1 then
                each(list, first, k - 1)
                first = k + 1
            end
            if text == ")" then
                depth = depth - 1
            end
        end
    end
end

local function escaped(text, escapes)
    return (text:gsub(".", escapes))
end

local function html(list)
    local out = {}
    for _, token in ipairs(list) do
        local text = escaped(token.text, kHtmlEscapes)
        local class = kHtml[token.role]
        out[#out + 1] = class and ('<span class="' .. class .. '">' .. text .. "</span>") or text
    end
    return '<div class="sourceCode signature"><pre class="sourceCode"><code class="sourceCode"><span>'
        .. table.concat(out)
        .. "</span></code></pre></div>"
end

local function latex(list)
    local out = {}
    for _, token in ipairs(list) do
        local text = escaped(token.text, kLatexEscapes)
        local macro = kLatex[token.role]
        if token.role == "name" then
            text = "\\textbf{" .. text .. "}"
        end
        out[#out + 1] = macro and ("\\" .. macro .. "{" .. text .. "}") or text
    end
    return "\\begin{biyossignature}\\begin{Highlighting}[]\n"
        .. table.concat(out)
        .. "\n\\end{Highlighting}\n\\end{biyossignature}"
end

local function marked(list, python, plain)
    if plain:match("^typedef") then
        return typedef(list)
    end
    local open = opening(list, 1)
    if open then
        head(list, open, python)
    end
    return open
end

local function signature(block)
    if not block.classes:includes("signature") then
        return nil
    end
    local list = tokens(block.text)
    local python = block.classes:includes("python")
    local open = marked(list, python, block.text)
    if open then
        parameters(list, open, python)
    end
    if quarto.doc.is_format("latex") then
        return pandoc.RawBlock("latex", latex(list))
    end
    return pandoc.RawBlock("html", html(list))
end

return {{CodeBlock = signature}}
