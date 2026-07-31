#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: object-mode circular reference table indexing.
#
# In mode: :object the `i` attribute is the index into the circular reference
# table and is fully attacker controlled. get_id_from_attrs() accumulated it
# into an unsigned long without any bound, so:
#
#   * circ_array_set() computed `id + 512` as the new table size, which wraps
#     for ids near ULONG_MAX. The table was then grown far too small and the
#     fill loop wrote past the end of it.
#   * circ_array_get() accepted id 0 (an element with no `i` attribute, or an
#     invalid one) and read objs[-1], outside the allocation, handing the word
#     back to Ruby as an object reference.
#
# Both must raise instead. The valid cases below guard against over tightening
# the new bound.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class ObjLoadCircularTest < ::Test::Unit::TestCase
  class Bag
    attr_accessor :a, :b

    def initialize(a, b)
      @a = a
      @b = b
    end
  end

  # id + 512 wraps, so the table is allocated with 411 slots and the fill loop
  # then writes Qundef from index 1 upwards with no end.
  def test_id_that_overflows_the_table_size_raises
    xml = %(<a i="1"><s i="#{2**64 - 101}">x</s></a>)
    assert_raise(Ox::ParseError) { Ox.parse_obj(xml) }
  end

  def test_id_at_the_very_top_of_the_range_raises
    [2**64 - 1, 2**64 - 512, 2**64 - 513, 2**63].each do |id|
      xml = %(<a i="1"><s i="#{id}">x</s></a>)
      assert_raise(Ox::ParseError, "id #{id} must be rejected") { Ox.parse_obj(xml) }
    end
  end

  # An id far larger than the document could ever have objects for is rejected
  # rather than allocating id + 512 slots for it.
  def test_id_beyond_the_document_length_raises
    xml = %(<a i="1"><s i="100000">x</s></a>)
    assert_raise(Ox::ParseError) { Ox.parse_obj(xml) }
  end

  def test_ref_without_an_id_attribute_raises
    assert_raise(Ox::ParseError) { Ox.parse_obj(%(<a i="1"><p/></a>)) }
  end

  def test_ref_with_a_zero_id_raises
    assert_raise(Ox::ParseError) { Ox.parse_obj(%(<a i="1"><p i="0"/></a>)) }
  end

  def test_ref_with_an_empty_or_non_numeric_id_raises
    ['', 'x', '-1', '1.5'].each do |id|
      assert_raise(Ox::ParseError, "id #{id.inspect} must be rejected") do
        Ox.parse_obj(%(<a i="1"><s i="2">x</s><p i="#{id}"/></a>))
      end
    end
  end

  def test_ref_past_the_end_of_the_table_raises
    assert_raise(Ox::ParseError) { Ox.parse_obj(%(<a i="1"><s i="2">x</s><p i="3"/></a>)) }
  end

  # Everything below must keep working.

  def test_valid_circular_reference_resolves
    a = Ox.parse_obj(%(<a i="1"><s i="2">x</s><p i="2"/></a>))
    assert_equal(%w[x x], a)
    assert_same(a[0], a[1])
  end

  def test_self_reference_resolves
    a = Ox.parse_obj(%(<a i="1"><p i="1"/></a>))
    assert_same(a, a[0])
  end

  def test_id_with_leading_zeros_resolves
    a = Ox.parse_obj(%(<a i="1"><s i="0000000000000000002">x</s><p i="00002"/></a>))
    assert_same(a[0], a[1])
  end

  # A base64 String element parks its id in pi->id for the following add_text,
  # which used to pass h->obj (still Qundef at that point) to circ_array_set
  # instead. The string was registered under the numeric value of Qundef, so no
  # reference to it could ever resolve.
  def test_base64_string_can_be_referenced
    a = Ox.parse_obj(%(<a i="1"><b i="2">aGVsbG8=</b><p i="2"/></a>))
    assert_equal(%w[hello hello], a)
    assert_same(a[0], a[1])
  end

  def test_dump_load_round_trip_preserves_sharing
    str = 'shared'
    bag = Bag.new(1, 2)
    obj = { 'one' => [str, str], 'two' => [bag, bag, str] }

    back = Ox.parse_obj(Ox.dump(obj, circular: true, indent: 2))
    assert_same(back['one'][0], back['one'][1])
    assert_same(back['one'][0], back['two'][2])
    assert_same(back['two'][0], back['two'][1])
    assert_equal('shared', back['one'][0])
  end

  # More objects than the table's initial 1024 slots, so the grow path runs.
  def test_round_trip_grows_the_table
    strs = Array.new(1500) { |i| "s#{i}" }
    obj  = strs + strs

    back = Ox.parse_obj(Ox.dump(obj, circular: true, indent: 2))
    assert_equal(3000, back.size)
    assert_same(back[0], back[1500])
    assert_same(back[1499], back[2999])
  end
end
