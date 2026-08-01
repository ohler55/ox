#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: object-mode containers that end_element() dereferences
# without checking what they hold.
#
# add_element() builds the container -- an Array for <a>, a Hash for <h>, three
# slots for <r>, a Struct for <u> -- and end_element() then uses it as that type.
# Three ways the container was not what it expected:
#
#   * add_text()'s default arm set h->obj = Qnil for every container type, so
#     text anywhere inside one left end_element() reading a nil back. <r>x</r>
#     alone segfaulted in RARRAY_PTR.
#   * skip: :skip_off hands the indentation between children to add_text(), so
#     the same nil landed under Ox.dump's own output.
#   * get_struct_from_attrs() returns Qundef when there is no c attribute to
#     name the Struct, and StructCode was the one arm not checking for it.
#
# The fourth is a type mixup rather than a container: get_var_sym_from_attrs()
# returns a Struct member index or an instance variable ID through one ID slot,
# and an ID is odd, so it satisfies FIXNUM_P and passed for an index.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class ObjLoadContainerTest < ::Test::Unit::TestCase
  # Named so it lands in Struct, which is the only place Ox looks a Struct up.
  Struct.new('OxPoint', :x, :y) unless Struct.const_defined?(:OxPoint)
  POINT = Struct::OxPoint

  CONTAINERS = {
    'Array'  => '<a>%s</a>',
    'Hash'   => '<h>%s</h>',
    'Range'  => '<r>%s</r>',
    'Struct' => '<u c="Struct::OxPoint">%s</u>',
  }.freeze

  def setup
    # The memcheck lane runs every file in one process, so do not inherit
    # whatever mode ran before this.
    @opts = Ox.default_options
    Ox.default_options = {mode: :object}
  end

  def teardown
    Ox.default_options = @opts
  end

  # Was a segfault for <r>, and a silent nil for the rest.
  def test_text_inside_a_container_raises
    CONTAINERS.each do |name, doc|
      assert_raise(Ox::ParseError, "#{name} must reject text") { Ox.parse_obj(doc % 'x') }
    end
  end

  def test_text_before_a_child_raises
    CONTAINERS.each do |name, doc|
      assert_raise(Ox::ParseError, "#{name} must reject text") do
        Ox.parse_obj(doc % 'x<i>1</i>')
      end
    end
  end

  # A container nested in another container has to fail the same way rather than
  # taking the parent down with it.
  def test_text_inside_a_nested_container_raises
    assert_raise(Ox::ParseError) { Ox.parse_obj('<a><r>x</r></a>') }
    assert_raise(Ox::ParseError) { Ox.parse_obj('<h><r>x</r><i>1</i></h>') }
  end

  # skip_off delivers the indentation between children as text. Every one of
  # these segfaulted, on Ox's own output.
  def test_indented_output_round_trips_with_skip_off
    # Setting the options replaces mode rather than merging it, so each call has
    # to name every option it needs.
    Ox.default_options = {mode: :object, indent: 2}
    values = [[1, 2], {1 => 2}, (1..2), POINT.new(1, 2), Complex(1, 2), Rational(1, 2)]
    dumped = values.map { |v| Ox.dump(v) }

    Ox.default_options = {mode: :object, skip: :skip_off}
    values.zip(dumped) { |v, xml| assert_equal(v, Ox.load(xml), "round trip of #{v.inspect}") }
  end

  # get_struct_from_attrs() returns Qundef here, which reached rb_struct_aset().
  def test_struct_without_a_class_attribute_raises
    assert_raise(Ox::ParseError) { Ox.parse_obj('<u><i a="0">1</i></u>') }
    assert_raise(Ox::ParseError) { Ox.parse_obj('<u><i a="x">1</i></u>') }
    assert_raise(Ox::ParseError) { Ox.parse_obj('<u/>') }
  end

  # An ID passed to rb_struct_aset() became FIX2LONG of itself, so a named
  # member picked whichever slot that number happened to be and the IndexError
  # reported the internal value.
  def test_struct_member_named_instead_of_numbered_raises
    %w[x foo @x].each do |name|
      assert_raise(Ox::ParseError, "member #{name.inspect} must be rejected") do
        Ox.parse_obj(%(<u c="Struct::OxPoint"><i a="#{name}">1</i></u>))
      end
    end
  end

  # atoi() wrapped these into a negative index, and Ruby counts a negative index
  # from the end of the struct, so "2147483648" wrote to the last member. The
  # values around 2**30 and 2**31 are here because FIXNUM_MAX is 2**30 - 1 where
  # long is 32 bits, so an index that big can not be handed to INT2NUM.
  def test_struct_index_past_int_max_does_not_wrap_negative
    ['16777217', '1073741823', '1073741824', '2147483647',
     '2147483648', '4294967296', '99999999999999999999'].each do |i|
      err = assert_raise(IndexError, "index #{i} must not wrap") do
        Ox.parse_obj(%(<u c="Struct::OxPoint"><i a="#{i}">1</i></u>))
      end
      assert_match(/too large/, err.message, "index #{i} became negative")
    end
  end

  # A numeric attribute became INT2NUM(n), which is odd and so is a valid ID.
  # Reaching rb_ivar_set() it named whatever had been interned into that slot.
  def test_numeric_attribute_does_not_set_an_instance_variable
    (1..40_000).step(1777) do |n|
      assert_raise(Ox::ParseError, "a=#{n} must be rejected") do
        Ox.parse_obj(%(<o c="Object"><i a="#{n}">1</i></o>))
      end
    end
  end

  # Same collision on the other side: <r> compares the attribute against @beg,
  # @end and @excl, and an index can equal one of those IDs.
  def test_numeric_attribute_is_not_a_range_bound
    (1..40_000).step(1777) do |n|
      assert_raise(Ox::ParseError, "a=#{n} must be rejected") do
        Ox.parse_obj(%(<r><i a="#{n}">1</i><i a="@end">2</i></r>))
      end
    end
  end

  # Everything below must keep working.

  def test_containers_still_load
    assert_equal([1, 2], Ox.parse_obj('<a><i>1</i><i>2</i></a>'))
    assert_equal({1 => 2}, Ox.parse_obj('<h><i>1</i><i>2</i></h>'))
    assert_equal((1..2), Ox.parse_obj('<r><i a="@beg">1</i><i a="@end">2</i><n a="@excl"/></r>'))
    assert_equal(POINT.new(1, 2),
                 Ox.parse_obj('<u c="Struct::OxPoint"><i a="0">1</i><i a="1">2</i></u>'))
  end

  def test_empty_containers_still_load
    assert_equal([], Ox.parse_obj('<a/>'))
    assert_equal({}, Ox.parse_obj('<h/>'))
  end

  def test_instance_variables_still_load
    o = Ox.parse_obj('<o c="Object"><i a="@a">1</i></o>')
    assert_equal(1, o.instance_variable_get(:@a))
  end

  # The index parse stops at INT_MAX rather than at the first non digit, so a
  # leading zero and a plain in range index have to keep working.
  def test_struct_index_forms_that_stay_valid
    doc = '<u c="Struct::OxPoint"><i a="%s">1</i></u>'
    assert_equal(POINT.new(1, nil), Ox.parse_obj(doc % '0'))
    assert_equal(POINT.new(1, nil), Ox.parse_obj(doc % '00'))
    assert_equal(POINT.new(nil, 1), Ox.parse_obj(doc % '1'))
  end
end
