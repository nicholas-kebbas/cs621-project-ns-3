# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

# def options(opt):
#     pass

# def configure(conf):
#     conf.check_nonfatal(header_name='stdint.h', define_name='HAVE_STDINT_H')

def build(bld):
    module = bld.create_ns3_module('cs621-project-ns-3', ['core', 'point-to-point', 'internet', 'applications'])
    module.source = [
        'model/cs621-project-ns-3.cc',
        'helper/cs621-project-ns-3-helper.cc',
        ]

    module_test = bld.create_ns3_module_test_library('cs621-project-ns-3')
    module_test.source = [
        'test/cs621-project-ns-3-test-suite.cc',
        ]

    headers = bld(features='ns3header')
    headers.module = 'cs621-project-ns-3'
    headers.source = [
        'model/cs621-project-ns-3.h',
        'helper/cs621-project-ns-3-helper.h',
        ]

    if bld.env.ENABLE_EXAMPLES:
        bld.recurse('examples')

    # bld.ns3_python_bindings()

