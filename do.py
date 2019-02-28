import bt2


g = bt2.Graph()

print('III')


class MySink(bt2._UserSinkComponent):
    def __init__(self, params):
        print('MySink __init__ ', params)
        self._add_input_port('in')

    def _consume(self):
        pass


class MySourceIter(bt2._UserMessageIterator):
    def __next__(self):
        raise bt2.Stop


class MySource(bt2._UserSourceComponent,
               message_iterator_class=MySourceIter):
    def __init__(self, params):
        print('MySource __init__ ', params)
        self._add_output_port('out')

    @staticmethod
    def _query(query_exec, obj, params):
        print('QUERY!', query_exec, obj, params)
        return {'obj': obj, 'params': params}


def port_added_listener(port):
    print('ADDED', port, port.name)


def ports_connected_listener(up, down):
    print('CONNECTED', up, up.name, down, down.name)


g.add_port_added_listener(port_added_listener)
g.add_ports_connected_listener(ports_connected_listener)

print('BBB')
src = g.add_source_component(MySource, 'asd')
print('CCC')
snk = g.add_sink_component(MySink, 'thesink')
g.connect_ports(src.output_ports['out'], snk.input_ports['in'])
