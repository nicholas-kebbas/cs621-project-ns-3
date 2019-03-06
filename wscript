# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

# def options(opt):
#     pass

# def configure(conf):
#     conf.check_nonfatal(header_name='stdint.h', define_name='HAVE_STDINT_H')

def build(bld):
    module = bld.create_ns3_module('project1', ['point-to-point','applications','core', 'internet', 'csma', 'config-store','stats'])
    module.source = [
        'model/project1.cc',
        'model/udp-app-client.cc',
        'model/udp-app-server.cc',
        'helper/udp-app-helper.cc',
        'helper/project1-helper.cc',
        ]

    module_test = bld.create_ns3_module_test_library('project1')
    module_test.source = [
        'test/project1-test-suite.cc',
        ]

    headers = bld(features='ns3header')
    headers.module = 'project1'
    headers.source = [
        'model/project1.h',
        'model/udp-app-client.h',
        'model/udp-app-server.h',
        'helper/udp-app-helper.h',
        'helper/project1-helper.h',
        ]

    if bld.env.ENABLE_EXAMPLES:
        bld.recurse('examples')

    # bld.ns3_python_bindings()

