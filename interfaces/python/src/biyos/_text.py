def encoded(text):
    if "\0" in text:
        raise ValueError("embedded null character")
    return text.encode()
