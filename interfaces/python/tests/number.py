class Number:
    def __init__(self, value):
        self.value = value

    def __index__(self):
        return self.value
