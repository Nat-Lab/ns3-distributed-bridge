def build(bld):
    module = bld.create_ns3_module('distributed-bridge', ['core'])
    module.source = [
        'model/distributed-bridge.cc',
        'helper/distributed-bridge-helper.cc',
        ]

    headers = bld(features='ns3header')
    headers.module = 'distributed-bridge'
    headers.source = [
        'model/dist-types.h',
        'model/distributed-bridge.h',
        'helper/distributed-bridge-helper.h',
        ]

