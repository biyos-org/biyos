from biyos._error import Error, check


class Relay:
    def __init__(self):
        self._raised = None
        self._depth = 0

    def run(self, function, *arguments):
        self._depth += 1
        try:
            function(*arguments)
        except BaseException as error:
            self._keep(error)
        finally:
            self._depth -= 1

    def idle(self):
        return self._depth == 0

    def start(self):
        if self._depth == 0:
            self._raised = None

    def finish(self, handle, status):
        raised = self._raised
        if self._depth > 0 or raised is None:
            return
        self._raised = None
        try:
            check(handle, status)
        except Error:
            raise raised
        else:
            raise raised
        finally:
            del raised

    def _keep(self, error):
        if self._raised is None:
            self._raised = error
