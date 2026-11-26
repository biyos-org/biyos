from biyos._convert.arrays import index_view


def listed(model, family, entries):
    view = index_view(len(entries))
    for k, entry in enumerate(entries):
        view[k] = entry
    return family(model, view, model._gen)
