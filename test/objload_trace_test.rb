#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: object mode :trace output overflowing its stack buffer.
#
# With :trace set, add_element() in obj_load.c formats the element name and its
# attributes into a char buf[1024] with s += snprintf(...). snprintf returns the
# length it wanted to write, so an attribute longer than the buffer moved s past
# the end and the following writes landed outside buf. A long enough attribute
# segfaulted.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class ObjLoadTraceTest < ::Test::Unit::TestCase
  def test_trace_long_attribute
    name = "@#{'x' * 200_000}"
    xml = %(<o c="Object"><s a="#{name}">hi</s></o>)
    obj = Ox.load(xml, mode: :object, trace: 1)
    assert_equal('hi', obj.instance_variable_get(name.to_sym))
  end
end
