import gdb.printing


class BtValueBasePrinter:
    def __init__(self, val, cast_to):
        self._val = val.cast(gdb.lookup_type(f'struct {cast_to}'))

    def display_hint(self):
        return None


class BtValueNullPrinter(BtValueBasePrinter):
    def __init__(self, val):
        super().__init__(val, 'bt_value')

    def to_string(self):
        return 'null'


class BtValueBoolPrinter(BtValueBasePrinter):
    def __init__(self, val):
        super().__init__(val, 'bt_value_bool')

    def to_string(self):
        return 'true' if self._val['value'] else 'false'


class BtValueUnsignedIntegerPrinter(BtValueBasePrinter):
    def __init__(self, val):
        super().__init__(val, 'bt_value_integer')

    def to_string(self):
        return self._val['value']['u']


class BtValueSignedIntegerPrinter(BtValueBasePrinter):
    def __init__(self, val):
        super().__init__(val, 'bt_value_integer')

    def to_string(self):
        return self._val['value']['i']


class BtValueRealPrinter(BtValueBasePrinter):
    def __init__(self, val):
        super().__init__(val, 'bt_value_real')

    def to_string(self):
        return self._val['value']


class BtValueStringPrinter(BtValueBasePrinter):
    def __init__(self, val):
        super().__init__(val, 'bt_value_string')

    def to_string(self):
        return self._val['gstr']['str']


class BtValueArrayPrinter(BtValueBasePrinter):
    def __init__(self, val):
        super().__init__(val, 'bt_value_array')

    def children(self):
        length = self._val['garray']['len']
        bt_value_type_ptr = gdb.lookup_type('struct bt_value').pointer()

        for i in range(length):
            void_ptr = self._val['garray']['pdata'][i]
            yield (str(i), void_ptr.cast(bt_value_type_ptr).dereference())

    def to_string(self):
        length = self._val['garray']['len']
        return f'array of length {length}'

    def display_hint(self):
        return 'array'


class BtValueMapPrinter(BtValueBasePrinter):
    def __init__(self, val):
        super().__init__(val, 'bt_value_map')

    def children(self):
        gdb.parse_and_eval('quarks')
        from glib_gdb import GHashPrinter, g_quark_to_string

        ghp = GHashPrinter(self._val['ght'])
        children_iter = ghp.children()
        bt_value_type_ptr = gdb.lookup_type('struct bt_value').pointer()

        try:
            while True:

                k1, k2 = next(children_iter)
                quark = int(k2)
                key_str = gdb.parse_and_eval('quarks')[quark]
                yield k1, key_str.string() if key_str else '?'
                v1, v2 = next(children_iter)
                yield v1, v2.cast(bt_value_type_ptr).dereference()
        except StopIteration:
            pass

    def to_string(self):
        return f'map of size ?'

    def display_hint(self):
        return 'map'


class BtValuePrinter:
    def __init__(self, val):
        type = val['type']

        if type == gdb.parse_and_eval('BT_VALUE_TYPE_NULL'):
            self._impl = BtValueNullPrinter(val)
        elif type == gdb.parse_and_eval('BT_VALUE_TYPE_BOOL'):
            self._impl = BtValueBoolPrinter(val)
        elif type == gdb.parse_and_eval('BT_VALUE_TYPE_UNSIGNED_INTEGER'):
            self._impl = BtValueUnsignedIntegerPrinter(val)
        elif type == gdb.parse_and_eval('BT_VALUE_TYPE_SIGNED_INTEGER'):
            self._impl = BtValueSignedIntegerPrinter(val)
        elif type == gdb.parse_and_eval('BT_VALUE_TYPE_REAL'):
            self._impl = BtValueRealPrinter(val)
        elif type == gdb.parse_and_eval('BT_VALUE_TYPE_STRING'):
            self._impl = BtValueStringPrinter(val)
        elif type == gdb.parse_and_eval('BT_VALUE_TYPE_ARRAY'):
            self._impl = BtValueArrayPrinter(val)
        elif type == gdb.parse_and_eval('BT_VALUE_TYPE_MAP'):
            self._impl = BtValueMapPrinter(val)
        else:
            raise RuntimeError('Invalid bt_value type')

        if hasattr(self._impl, 'children'):

            def children():
                return self._impl.children()

            setattr(self, 'children', children)

    def to_string(self):
        return self._impl.to_string()

    def display_hint(self):
        return self._impl.display_hint()


pp = gdb.printing.RegexpCollectionPrettyPrinter("libbabeltrace")
pp.add_printer('bt_value', '^bt_value$', BtValuePrinter)


gdb.printing.register_pretty_printer(None, pp)
