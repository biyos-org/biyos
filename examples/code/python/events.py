import logging
from functools import partial

import biyos
from biyos import EventKind, EventLevel

LEVELS = {
    EventLevel.DEBUG: logging.DEBUG,
    EventLevel.INFO: logging.INFO,
    EventLevel.WARNING: logging.WARNING,
    EventLevel.ERROR: logging.ERROR,
}

logging.basicConfig(format="%(name)s %(levelname)s %(message)s")
logger = logging.getLogger("biyos")


def forward(event):
    logger.log(LEVELS[event.level], "%s", event.fmttext())


def keep(seen, event):
    seen.append((event.module, event.text, dict(event.fields)))


seen = []
with biyos.Env() as env:
    env.addhook(forward, EventKind.MESSAGE, EventLevel.INFO)
    env.addhook(partial(keep, seen), EventKind.MESSAGE, EventLevel.ERROR)
    try:
        env.setopt("seed", -1)
    except biyos.Error as error:
        print(error.code, error)

module, text, fields = seen[0]
print(module, fields)
assert module == "capi" and fields == {"code": "-100"}
