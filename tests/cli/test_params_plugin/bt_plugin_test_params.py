import bt2


class MySourceIter(bt2._UserMessageIterator):
    pass


@bt2.plugin_component_class
class MySource(bt2._UserSourceComponent, message_iterator_class=MySourceIter):
    def __init__(self, params):
        self._add_output_port('sortie')


@bt2.plugin_component_class
class MySink(bt2._UserSinkComponent):
    def __init__(self, params):
        self._p = self._add_input_port('entree')
        print(params)

    def _consume(self):
        next(self._i)

    def _graph_is_configured(self):
        self._i = self._p.create_message_iterator()


bt2.register_plugin(__name__, 'test_params')
