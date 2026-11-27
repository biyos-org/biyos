from operator import index

from biyos._convert.arrays import index_view
from biyos._model.blocks import Block, View


def listed(model, family, entries):
    view = index_view(len(entries))
    for k, entry in enumerate(entries):
        view[k] = entry
    return family(model, view, model._gen)


def indexed(found):
    if isinstance(found, (tuple, list)):
        return type(found)(map(indexed, found))
    if isinstance(found, Block):
        return found.indices()
    if isinstance(found, View):
        return index(found)
    return found
