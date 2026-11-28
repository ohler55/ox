#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: Ox.dump reading an attribute key before calling to_s on the
# value, which can change the key String.

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class DumpGenAttrKeyTest < ::Test::Unit::TestCase
  def test_key_grown_by_value_to_s
    key = 'k' * 1000
    value = Object.new
    value.define_singleton_method(:to_s) do
      key << ('y' * 100_000)
      GC.start
      'v'
    end
    xml = Ox.dump(element_with(key, value))
    assert_equal(%(\n<a #{key}="v"/>\n), xml)
  end

  def test_key_changed_to_a_bad_name_by_value_to_s
    key = +'k'
    value = Object.new
    value.define_singleton_method(:to_s) do
      key.replace('k="1" b')
      'v'
    end
    assert_raise(Ox::SyntaxError) { Ox.dump(element_with(key, value)) }
  end

  private

  # compare_by_identity keeps the key String itself, so to_s can change it.
  def element_with(key, value)
    attrs = {}.compare_by_identity
    attrs[key] = value
    e = Ox::Element.new('a')
    e.instance_variable_set(:@attributes, attrs)
    e
  end
end
