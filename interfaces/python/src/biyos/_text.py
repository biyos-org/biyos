def encoded(text: str) -> bytes:
    if "\0" in text:
        raise ValueError("embedded null character")
    return text.encode()


def text(name: str, value: str) -> bytes:
    if not isinstance(value, str):
        found = type(value).__name__
        raise TypeError(f"{name} must be a str, not {found}")
    return encoded(value)


def decoded(value: bytes | None) -> str | None:
    return value.decode() if value else None
