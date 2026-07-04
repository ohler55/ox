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
