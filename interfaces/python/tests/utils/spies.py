import sys
from contextlib import contextmanager


def run(actions):
    while actions:
        action = actions.pop(0)
        action()


def preceded(actions, function, *arguments):
    run(actions)
    return function(*arguments)


def followed(actions, function, *arguments):
    result = function(*arguments)
    run(actions)
    return result


def interrupted(function, *arguments):
    function(*arguments)
    raise KeyboardInterrupt


def spied(function, statuses, *arguments):
    status = function(*arguments)
    statuses.append(status)
    return status


class Trap:
    def __init__(self, function):
        self.code = function.__code__
        self.armed = False

    def arm(self):
        self.armed = True

    def enter(self, frame, event, argument):
        return self.line if frame.f_code is self.code else None

    def line(self, frame, event, argument):
        if self.armed and event == "line":
            self.armed = False
            raise KeyboardInterrupt
        return self.line


@contextmanager
def traced(trap):
    previous = sys.gettrace()
    sys.settrace(trap.enter)
    try:
        yield
    finally:
        sys.settrace(previous)
