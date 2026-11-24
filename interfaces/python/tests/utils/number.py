class Number:
    def __init__(self, value):
        self.value = value

    def __index__(self):
        return self.value


class Growing:
    def __init__(self, value, grow):
        self.value = value
        self.grow = grow

    def __index__(self):
        self.grow()
        return self.value
