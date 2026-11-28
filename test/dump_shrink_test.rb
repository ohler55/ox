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
    value = Object.new
    value.define_singleton_method(:to_s) do
      arr.replace([])
      GC.start
      'v'
    end
    first = Ox::Element.new('a')
    first.attributes[:x] = value
    arr = [first] + Array.new(100) { |i| "s#{i}" }
    assert_equal(1, Ox.load(Ox.dump(arr, mode: :object), mode: :object).size)
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

  def test_nodes_shrunk_to_end_with_text
    root = Ox::Element.new('root')
    value = Object.new
    value.define_singleton_method(:to_s) do
      root.nodes.replace(root.nodes[0, 2])
      'v'
    end
    first = Ox::Element.new('a')
    first.attributes[:x] = value
    root << first << 'text' << 'more'
    assert_equal(%(\n<root>\n  <a x="v"/>text</root>\n), Ox.dump(root))
  end

  def test_nodes_shrunk_to_end_with_an_instruction
    root = Ox::Element.new('root')
    value = Object.new
    value.define_singleton_method(:to_s) do
      root.nodes.pop
      'v'
    end
    pi = Ox::Instruct.new('pi')
    pi.attributes[:k] = value
    root << pi << Ox::Element.new('b')
    assert_equal(%(\n<root><?pi k="v"?></root>\n), Ox.dump(root))
  end
end
