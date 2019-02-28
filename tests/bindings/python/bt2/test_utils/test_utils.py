import bt2


def run_in_component_init(func):
    class MySink(bt2._UserSinkComponent):
        def __init__(self, params):
            nonlocal res
            res = func(self)

        def _consume(self):
            pass

    res = None

    g = bt2.Graph()
    g.add_sink_component(MySink, 'comp')
    return res


def get_dummy_trace_class():
    def f(comp_self):
        return comp_self._create_trace_class()

    return run_in_component_init(f)
