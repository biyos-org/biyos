from biyos import _header


def group(prefix):
    names = vars(_header)
    return {name: names[name] for name in names if name.startswith(prefix)}
