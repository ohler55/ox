#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: object-mode class name stack overflow.
#
# In mode: :object the `c` attribute of an object element is the class name and
# is fully attacker controlled. classname2class() copied it into a fixed
# char class_name[1024] without a bounds check, so a class name longer than
# 1024 bytes smashed the stack. It must raise instead.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class ObjLoadClassnameTest < ::Test::Unit::TestCase
  def test_over_long_classname_raises
    xml = %(<o c="#{'A' * 5000}"><i a="@x">1</i></o>)
    assert_raise(Ox::ParseError) do
      Ox.load(xml, mode: :object, effort: :tolerant)
    end
  end

  def test_boundary_classname_raises
    # Just past the 1024-byte buffer.
    xml = %(<o c="#{'A' * 1100}"><i a="@x">1</i></o>)
    assert_raise(Ox::ParseError) do
      Ox.load(xml, mode: :object, effort: :tolerant)
    end
  end

  def test_short_undefined_class_still_raises_cleanly
    # A normal short undefined class name must raise cleanly (NameError from
    # the constant lookup), never crash.
    assert_raise(NameError) do
      Ox.load(%(<o c="NoSuchClassXYZ"><i a="@x">1</i></o>), mode: :object, effort: :strict)
    end
  end
end

# Regression test: object-mode struct name over-read.
#
# In mode: :object the `c` attribute of a struct element (<u>) is the struct
# class name. structname2obj() stepped over a "::" namespace separator with an
# unconditional `s += 2`, so a name ending in a single ':' skipped past the
# terminating '\0' and rb_intern() read on into the bytes that followed the
# attribute in the document. The constant lookup name must stay inside the
# attribute value.
class ObjLoadStructNameTest < ::Test::Unit::TestCase
  def test_struct_name_trailing_colon_does_not_read_past_token
    xml = %(<u c="Foo:">PAST_THE_TOKEN</u>)
    e = assert_raise(NameError) { Ox.load(xml, mode: :object) }
    assert(!e.message.include?('PAST_THE_TOKEN'),
           "struct class name read past the attribute value: #{e.message.inspect}")
  end

  def test_namespaced_struct_still_loads
    Struct.new('OxStructNameRegress', :x) unless defined?(Struct::OxStructNameRegress)
    s = Struct::OxStructNameRegress.new(7)
    assert_equal(s, Ox.load(Ox.dump(s, mode: :object), mode: :object))
  end
end
