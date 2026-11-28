#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: Ox.dump walking an Array or the nodes of an element through
# a pointer taken before Ruby code that shrinks the Array runs.

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class DumpShrinkTest < ::Test::Unit::TestCase
  def test_array_shrunk_by_an_element
    arr = nil
    err = begin
      undefined_name_for_ox_test
    rescue NameError => e
      e
    end
    err.define_singleton_method(:message) do
      arr.replace([])
      GC.start
      super()
    end
    arr = [err] + Array.new(100) { |i| "s#{i}" }
    back = Ox.load(Ox.dump(arr, mode: :object), mode: :object)
    # TruffleRuby does not call #message here.
    assert_equal(1, back.size) if RUBY_ENGINE == 'ruby'
  end

  def test_nodes_shrunk_by_an_attribute
    root = Ox::Element.new('root')
    value = Object.new
    value.define_singleton_method(:to_s) do
      root.nodes.replace([])
      GC.start
      'v'
    end
    first = Ox::Element.new('a')
    first.attributes[:x] = value
    root << first
    100.times { |i| root << Ox::Element.new("e#{i}") }
    assert_equal(%(\n<root>\n  <a x="v"/>\n</root>\n), Ox.dump(root))
  end
end
