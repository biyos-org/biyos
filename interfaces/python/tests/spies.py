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
