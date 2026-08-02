#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: Ox::Builder#element did not validate the element name.
#
# Two defects, both in builder_element().
#
#   * A name of 64 bytes or more is copied with strdup(), which stops at the
#     first embedded NUL, but e->len keeps the full RSTRING_LEN. append_string()
#     then runs xml_str_len() over len bytes of a strlen()+1 sized block, so the
#     read runs as far past the allocation as the caller cares to make the name.
#     ASAN reports heap-buffer-overflow READ of size 8 in xml_str_len().
#
#   * The "expected a Symbol or String" raise happened after b->depth++, so a
#     rescued call left the Builder holding a stack slot that was never filled
#     in. The next call wrote through it and builder_free() later free()d
#     whatever the uninitialised name pointer happened to be. Same shape as the
#     depth raise in builder_depth_test.rb, reached by a different raise.

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class BuilderNameTest < ::Test::Unit::TestCase
  # struct _element's inline buf. At or above this the name is strdup'd.
  INLINE_BUF = 64

  def test_nul_in_a_short_element_name_raises
    b = Ox::Builder.new
    assert_raise(Ox::ArgError) { b.element("a\0bcd") }
  end

  def test_nul_in_a_strdup_length_element_name_raises
    b = Ox::Builder.new
    assert_raise(Ox::ArgError) { b.element("\0#{'x' * 200_000}") }
    assert_raise(Ox::ArgError) { b.element("#{'a' * 70}\0#{'b' * 200_000}") }
  end

  # The bug lives on the strdup side of this boundary, so pin both sides of it
  # and make sure the fix did not move the threshold.
  def test_names_around_the_inline_buffer_boundary
    [INLINE_BUF - 2, INLINE_BUF - 1, INLINE_BUF, INLINE_BUF + 1, 200, 5000].each do |n|
      name = 'a' * n
      xml  = Ox::Builder.new { |b| b.element(name) { b.text('t') } }
      assert_equal("<#{name}>t</#{name}>\n", xml, "name length #{n}")
    end
  end

  def test_symbol_names_still_work
    assert_equal("<abc/>\n", Ox::Builder.new { |b| b.element(:abc) })
  end

  # On the unfixed code the raise leaves b->depth pointing at a slot that was
  # never written, so the next element() reads its has_child and closes that
  # phantom element with a stray '>' ahead of its own tag.
  def test_builder_is_usable_after_a_rescued_name_type_raise
    b = Ox::Builder.new
    assert_raise(Ox::ArgError) { b.element(123) }
    b.element('x')
    b.pop
    assert_equal("<x/>\n", b.to_s)
  end

  def test_builder_is_usable_after_a_rescued_nul_name_raise
    b = Ox::Builder.new
    assert_raise(Ox::ArgError) { b.element("a\0b") }
    b.element('x')
    b.pop
    assert_equal("<x/>\n", b.to_s)
  end

  def test_many_rescued_name_raises_then_reuse
    b = Ox::Builder.new
    200.times do
      begin
        b.element(123)
      rescue Ox::ArgError
        nil
      end
    end
    b.element('x')
    b.pop
    assert_equal("<x/>\n", b.to_s)
  end

  # builder_free() walks the stack down from b->depth and free()s any name that
  # is not the inline buffer, so a slot left uninitialised by a rescued raise is
  # only released here.
  def test_gc_after_rescued_name_raises
    assert_nothing_raised do
      50.times do
        b = Ox::Builder.new
        begin
          b.element(:not_a_name_type.to_proc)
        rescue Ox::ArgError
          nil
        end
        b = nil
        GC.start
      end
    end
  end

  def test_valid_output_unchanged
    xml = Ox::Builder.new do |b|
      b.instruct('xml', version: '1.0')
      b.element('top', 'a' => '1') do
        b.element('mid') { b.text('hello') }
        b.comment('note')
        b.void_element('br')
      end
    end
    expected = <<~XML
      <?xml version="1.0"?>
      <top a="1">
        <mid>hello</mid>
        <!--note-->
        <br>
      </top>
    XML
    assert_equal(expected, xml)
  end
end
