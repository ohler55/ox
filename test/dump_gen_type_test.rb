#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: Ox.dump using the @nodes and @attributes of generic nodes as
# an Array and a Hash without checking their type.

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class DumpGenTypeTest < ::Test::Unit::TestCase
  def test_element_nodes_not_an_array
    e = Ox::Element.new('a')
    e.instance_variable_set(:@nodes, 'abcdefghijklmnopqrstuvwxyz')
    assert_raise(TypeError) { Ox.dump(e) }
  end

  def test_document_nodes_not_an_array
    doc = Ox::Document.new
    doc.instance_variable_set(:@nodes, 7)
    assert_raise(TypeError) { Ox.dump(doc) }
  end

  def test_element_attributes_not_a_hash
    e = Ox::Element.new('a')
    e.instance_variable_set(:@attributes, 7)
    assert_raise(TypeError) { Ox.dump(e) }
  end

  def test_document_attributes_not_a_hash
    doc = Ox::Document.new
    doc.instance_variable_set(:@attributes, 7)
    assert_raise(TypeError) { Ox.dump(doc) }
    assert_raise(TypeError) { Ox.dump(doc, encoding: 'UTF-8', with_xml: true) }
  end

  def test_instruct_attributes_not_a_hash
    pi = Ox::Instruct.new('pi')
    pi.instance_variable_set(:@attributes, 7)
    root = Ox::Element.new('root')
    root << pi
    assert_raise(TypeError) { Ox.dump(root) }
  end

  def test_types_from_object_mode
    nodes = %(<o c="Ox::Element"><s a="@value">a</s><s a="@nodes">abcdefghijklmnopqrstuvwxyz</s></o>)
    attributes = %(<o c="Ox::Element"><s a="@value">a</s><i a="@attributes">7</i></o>)
    assert_raise(TypeError) { Ox.dump(Ox.load(nodes, mode: :object)) }
    assert_raise(TypeError) { Ox.dump(Ox.load(attributes, mode: :object)) }
  end
end
